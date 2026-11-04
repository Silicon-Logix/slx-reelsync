// Silicon LogiX / SLX ReelSync — adapted from the SolarPilot renderer
// Copyright (c) 2026 Marco Pezzullo. Author: Marco Pezzullo.
// SPDX-License-Identifier: LicenseRef-SLX-ReelSync-Demo-Evaluation-1.0
#include "renderer.hpp"
#include "alarms.hpp"
#include "gl_api.hpp"
#include "instrument_panel.hpp"
#define CGLTF_IMPLEMENTATION
#include <cgltf.h>
#define STB_IMAGE_IMPLEMENTATION
#include <stb_image.h>
#define STB_IMAGE_WRITE_IMPLEMENTATION
#include <algorithm>
#include <array>
#include <chrono>
#include <cmath>
#include <fstream>
#include <map>
#include <set>
#include <stb_image_write.h>
namespace slx::desktop {
namespace {
struct V3 {
  float x = 0, y = 0, z = 0;
  V3 operator+(V3 b) const {
    return {x + b.x, y + b.y, z + b.z};
  }
  V3 operator-(V3 b) const {
    return {x - b.x, y - b.y, z - b.z};
  }
  V3 operator*(float s) const {
    return {x * s, y * s, z * s};
  }
};
float dot(V3 a, V3 b) {
  return a.x * b.x + a.y * b.y + a.z * b.z;
}
V3 cross(V3 a, V3 b) {
  return {a.y * b.z - a.z * b.y, a.z * b.x - a.x * b.z, a.x * b.y - a.y * b.x};
}
V3 norm(V3 v) {
  return v * (1 / std::sqrt(std::max(1e-9f, dot(v, v))));
}
struct M4 {
  float m[16]{};
  static M4 identity() {
    M4 v;
    v.m[0] = v.m[5] = v.m[10] = v.m[15] = 1;
    return v;
  }
};
M4 mul(const M4 &a, const M4 &b) {
  M4 c;
  for (int col = 0; col < 4; ++col)
    for (int row = 0; row < 4; ++row)
      for (int k = 0; k < 4; ++k)
        c.m[col * 4 + row] += a.m[k * 4 + row] * b.m[col * 4 + k];
  return c;
}
V3 point(const M4 &m, V3 v) {
  return {m.m[0] * v.x + m.m[4] * v.y + m.m[8] * v.z + m.m[12],
          m.m[1] * v.x + m.m[5] * v.y + m.m[9] * v.z + m.m[13],
          m.m[2] * v.x + m.m[6] * v.y + m.m[10] * v.z + m.m[14]};
}
M4 look(V3 eye, V3 center, V3 up) {
  V3 f = norm(center - eye), s = norm(cross(f, up)), u = cross(s, f);
  auto a = M4::identity();
  a.m[0] = s.x;
  a.m[4] = s.y;
  a.m[8] = s.z;
  a.m[1] = u.x;
  a.m[5] = u.y;
  a.m[9] = u.z;
  a.m[2] = -f.x;
  a.m[6] = -f.y;
  a.m[10] = -f.z;
  a.m[12] = -dot(s, eye);
  a.m[13] = -dot(u, eye);
  a.m[14] = dot(f, eye);
  return a;
}
M4 perspective(float aspect, float hfov, float near_z, float far_z) {
  M4 a;
  const float f = aspect / std::tan(hfov / 2);
  a.m[0] = f / aspect;
  a.m[5] = f;
  a.m[10] = -(far_z + near_z) / (far_z - near_z);
  a.m[11] = -1;
  a.m[14] = -2 * far_z * near_z / (far_z - near_z);
  return a;
}
unsigned shader_program(const char *vs, const char *fs) {
  auto shader = [](unsigned type, const char *text) {
    auto s = gl::CreateShader(type);
    gl::ShaderSource(s, 1, &text, nullptr);
    gl::CompileShader(s);
    int ok;
    gl::GetShaderiv(s, GL_COMPILE_STATUS, &ok);
    if (!ok) {
      char e[4096];
      gl::GetShaderInfoLog(s, 4096, nullptr, e);
      throw std::runtime_error(e);
    }
    return s;
  };
  auto v = shader(GL_VERTEX_SHADER, vs), f = shader(GL_FRAGMENT_SHADER, fs),
       p = gl::CreateProgram();
  gl::AttachShader(p, v);
  gl::AttachShader(p, f);
  gl::LinkProgram(p);
  gl::DeleteShader(v);
  gl::DeleteShader(f);
  int ok;
  gl::GetProgramiv(p, GL_LINK_STATUS, &ok);
  if (!ok)
    throw std::runtime_error("GLSL link failed");
  return p;
}
const char *vertex = R"(#version 330 core
layout(location=0)in vec3 pos;layout(location=1)in vec3 normal;layout(location=2)in vec2 uv;
uniform mat4 model,vp,lightVP;out vec3 N,W;out vec2 UV;out vec4 LightPos;
void main(){vec4 w=model*vec4(pos,1);W=w.xyz;N=mat3(transpose(inverse(model)))*normal;UV=uv;LightPos=lightVP*w;gl_Position=vp*w;})";
const char *fragment = R"(#version 330 core
in vec3 N,W;in vec2 UV;in vec4 LightPos;out vec4 color;uniform vec4 base;uniform vec3 eye,emission,axisCenter;
uniform float metallic,roughness,daylight,cutoff,exposure,keyLight;uniform sampler2D tex,emTex;uniform sampler2DShadow shadowTex;
uniform int hasTex,hasEmTex,shadowEnabled,selected,layered,webSurface,instrumentSurface,attention;
uniform float markerPitch;
vec3 aces(vec3 x){return clamp((x*(2.51*x+.03))/(x*(2.43*x+.59)+.14),0.,1.);}
float visibility(vec3 n){vec3 p=LightPos.xyz/LightPos.w*.5+.5;
 if(shadowEnabled==0||p.x<0.||p.x>1.||p.y<0.||p.y>1.||p.z>1.)return 1.;
 float bias=max(.00035,.0012*(1.-dot(n,normalize(vec3(-.45,1.,.65))))),s=0.;vec2 stepUV=1./vec2(textureSize(shadowTex,0));
 for(int x=-2;x<=2;x++)for(int y=-2;y<=2;y++){float weight=float((3-abs(x))*(3-abs(y)));s+=weight*texture(shadowTex,vec3(p.xy+vec2(x,y)*stepUV,p.z-bias));}return s/81.;}
vec3 brdf(vec3 n,vec3 v,vec3 l,vec3 b){vec3 h=normalize(l+v);float nl=max(dot(n,l),0.),nv=max(dot(n,v),.001),nh=max(dot(n,h),0.),vh=max(dot(v,h),0.);
 float a=max(.07,roughness*roughness),a2=a*a,d=a2/(3.14159*pow(nh*nh*(a2-1.)+1.,2.)),k=pow(roughness+1.,2.)/8.;
 float g=(nv/(nv*(1.-k)+k))*(nl/(nl*(1.-k)+k));vec3 f0=mix(vec3(.04),b,metallic),f=f0+(1.-f0)*pow(1.-vh,5.);
 return ((1.-f)*(1.-metallic)*b/3.14159+d*g*f/max(.004,4.*nv*nl))*nl;}
void main(){vec4 b=base*(hasTex==1?texture(tex,UV):vec4(1));if(b.a<cutoff)discard;
 if(instrumentSurface==1){color=vec4(pow(max(b.rgb,vec3(0)),vec3(1./2.2)),b.a);return;}
 if(webSurface==1){float edge=abs(UV.y*2.-1.);
  float phase=UV.x/markerPitch+.25*edge;
  float stripe=abs(fract(phase+.5)-.5),aa=max(fwidth(phase),.003);
  float ink=(1.-smoothstep(.035-aa,.035+aa,stripe))*(1.-smoothstep(.81,.85,edge));
  float tick=(1.-smoothstep(.018-aa,.018+aa,abs(fract(UV.x/markerPitch+.5)-.5)))*smoothstep(.90,.93,edge);
  b.rgb=mix(b.rgb,vec3(.012,.028,.06),clamp(ink+tick,0.,1.));}
 vec3 n=normalize(N);if(!gl_FrontFacing)n=-n;vec3 v=normalize(eye-W);
 if(layered==1){float r=length((W-axisCenter).xy);float detail=1.-smoothstep(.0003,.003,fwidth(r));b.rgb*=1.-.08*detail*(.5+.5*cos(r*4200.));}
 vec3 ambient=mix(vec3(.10,.12,.16),vec3(.25,.29,.34),n.y*.5+.5);
 vec3 lit=b.rgb*ambient+brdf(n,v,normalize(vec3(-.45,1.,.65)),b.rgb)*2.8*keyLight*visibility(n)
  +brdf(n,v,normalize(vec3(.8,.45,-.5)),b.rgb)*vec3(.9,1.1,1.35)
  +brdf(n,v,normalize(vec3(-.7,.5,-.8)),b.rgb)*vec3(.65,.72,.85)
  +emission*(hasEmTex==1?texture(emTex,UV).rgb:vec3(1));
 if(selected==1)lit+=vec3(.04,.18,.4)*pow(1.-abs(dot(n,v)),2.);
 if(attention>0){vec3 a=attention==2?vec3(1.,.035,.02):vec3(1.,.42,.025);
  lit=mix(lit,a*.8,.38)+a*(.28+.7*pow(1.-abs(dot(n,v)),2.));}
 color=vec4(pow(aces(lit*exposure),vec3(1./2.2)),b.a);})";
const char *depthVertex = R"(#version 330 core
layout(location=0)in vec3 pos;uniform mat4 model,lightVP;
void main(){gl_Position=lightVP*model*vec4(pos,1);})";
const char *depthFragment = R"(#version 330 core
void main(){})";
M4 ortho(float r) {
  M4 m = M4::identity();
  m.m[0] = m.m[5] = 1 / r;
  m.m[10] = -2 / 45.f;
  m.m[14] = -1;
  return m;
}
V3 linearRGB(unsigned value) {
  auto c = [](unsigned v) {
    float x = v / 255.f;
    return x <= .04045f ? x / 12.92f : std::pow((x + .055f) / 1.055f, 2.4f);
  };
  return {c((value >> 16) & 255), c((value >> 8) & 255), c(value & 255)};
}
json read_json(const std::filesystem::path &p) {
  std::ifstream f(p);
  if (!f)
    throw std::runtime_error("Missing contract: " + p.string());
  return json::parse(f);
}
} // namespace
struct RendererImpl {
  struct Model {
    cgltf_data *data = nullptr;
    M4 placement = M4::identity();
    std::string kind;
    std::vector<M4> world;
    std::vector<bool> visible;
    std::map<std::string, bool> overrides;
    std::vector<std::array<float, 3>> positions, scales;
    std::vector<std::array<float, 4>> rotations;
    std::vector<std::vector<float>> weights, defaults;
    ~Model() {
      cgltf_free(data);
    }
  };
  struct Draw {
    Model *model = nullptr;
    size_t node = 0;
    unsigned vao = 0, vbo = 0, ibo = 0, tex = 0, emtex = 0;
    int count = 0;
    float base[4]{1, 1, 1, 1};
    float metal = 0, rough = .8f, cutoff = .01f;
    V3 emission, min, max;
    bool transparent = false, double_sided = false;
    std::string material;
    std::vector<float> base_vertices;
    std::vector<std::vector<float>> morphs;
  };
  std::vector<std::unique_ptr<Model>> models;
  std::vector<Draw> draws;
  std::map<const cgltf_texture *, unsigned> textures;
  std::vector<unsigned> product_images;
  std::set<std::string> hidden, south10;
  json visuals;
  float exposure = 1.08f, key_light = 1;
  int style = 1;
  bool hide_guards = false, hide_hall = false, shadows = true;
  bool web_markers = true;
  float marker_pitch = .5f;
  json clip_tracking;
  unsigned live_hmi = 0, live_gauge = 0;
  json instrument_stamp;
  int instrument_updates = 0;
  double instrument_force = 0, instrument_time = 0;
  unsigned shadow_fbo = 0, shadow_tex = 0, shadow_shader = 0;
  int shadow_draws = 0;
  M4 light_vp;
  int solid_support_draws = 0, joint_faces = 0;
  int rail_repaired_vertices = 0;
  int support_winding_errors = 0;
  float near_plane = .1f, far_plane = 80;
  std::map<std::string, int> surface_repair_counts;
  json articulations = json::array();
  unsigned shader = 0, fbo = 0, target = 0, depth = 0, ms_fbo = 0, ms_color = 0;
  int width = 0, height = 0, triangles = 0;
  double render_ms = 0, load_ms = 0;
  V3 center{0, 2, 0}, eye;
  float yaw = .6f, pitch = .45f, radius = 13, hfov = 1.0f;
  V3 up{0, 1, 0}, desired_center{0, 1.6f, 0};
  float desired_yaw = .52f, desired_pitch = .3f, desired_radius = 15;
  bool fit_requested = true;
  json cameras;
  int live_path_vertices = 0, morph_draws = 0;
  int attention_draws = 0;
  M4 vp;
  bool cutaway = false;
  json state;
  void upload(unsigned &texture, const PanelPixels &pixels) {
    gl::ActiveTexture(GL_TEXTURE0);
    if (!texture) {
      gl::GenTextures(1, &texture);
      product_images.push_back(texture);
    }
    gl::BindTexture(GL_TEXTURE_2D, texture);
    gl::TexImage2D(GL_TEXTURE_2D, 0, GL_SRGB8_ALPHA8, pixels.width, pixels.height, 0, GL_RGBA,
                   GL_UNSIGNED_BYTE, pixels.rgba.data());
    gl::TexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    gl::TexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    gl::TexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    gl::TexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
  }
  void instruments(const json &state, bool presentation) {
    const auto t = state.value("telemetry", json::object());
    const auto code = t.empty() ? SLX_RS_READY : t.at("state").at("code").get<uint32_t>();
    const double seconds = t.value("sim_s", 0.);
    const bool advancing =
        code == SLX_RS_RUNNING || code == SLX_RS_STOPPING || code == SLX_RS_FAULT;
    json stamp = {{"tick", int64_t(std::floor(seconds * 10))},
                  {"state", code},
                  {"presentation", presentation},
                  {"alarms", t.value("alarms", json::object())},
                  {"held", advancing ? json(nullptr) : t}};
    if (stamp == instrument_stamp)
      return;
    instrument_stamp = stamp;
    instrument_time = seconds;
    instrument_force = t.empty() ? 0. : t.at("process").at("pneumatic_force_n").get<double>();
    upload(live_hmi, operatorPanel(t, presentation));
    upload(live_gauge, forceDial(instrument_force, presentation || t.empty()));
    ++instrument_updates;
  }
  void gaugeOverlay() {
    auto &m = *models.front();
    size_t node = m.data->nodes_count;
    for (size_t i = 0; i < m.data->nodes_count; ++i)
      if (m.data->nodes[i].name &&
          std::string(m.data->nodes[i].name) == "rs_pneumatic_service_root")
        node = i;
    if (node == m.data->nodes_count)
      throw std::runtime_error("Supporto strumenti non disponibile nella scena.");
    Draw d;
    d.model = &m;
    d.node = node;
    d.material = "RS_live_force_display";
    d.count = 6;
    d.cutoff = .1f;
    const float x = 1.95f, y = .88f, z = -1.092f, r = .045f;
    d.base_vertices = {x - r, y - r, z, 0, 0, -1, 1, 1, x + r, y - r, z, 0, 0, -1, 0, 1,
                       x + r, y + r, z, 0, 0, -1, 0, 0, x - r, y + r, z, 0, 0, -1, 1, 0};
    d.min = {x - r, y - r, z};
    d.max = {x + r, y + r, z};
    const unsigned indices[] = {0, 2, 1, 0, 3, 2};
    gl::GenVertexArrays(1, &d.vao);
    gl::BindVertexArray(d.vao);
    gl::GenBuffers(1, &d.vbo);
    gl::BindBuffer(GL_ARRAY_BUFFER, d.vbo);
    gl::BufferData(GL_ARRAY_BUFFER, d.base_vertices.size() * sizeof(float), d.base_vertices.data(),
                   GL_STATIC_DRAW);
    gl::GenBuffers(1, &d.ibo);
    gl::BindBuffer(GL_ELEMENT_ARRAY_BUFFER, d.ibo);
    gl::BufferData(GL_ELEMENT_ARRAY_BUFFER, sizeof(indices), indices, GL_STATIC_DRAW);
    for (unsigned i = 0; i < 3; ++i) {
      gl::EnableVertexAttribArray(i);
      gl::VertexAttribPointer(i, i == 2 ? 2 : 3, GL_FLOAT, GL_FALSE, 8 * sizeof(float),
                              reinterpret_cast<void *>(size_t(i == 0   ? 0
                                                              : i == 1 ? 3
                                                                       : 6) *
                                                       sizeof(float)));
    }
    draws.push_back(std::move(d));
    triangles += 2;
  }
  void load(const std::filesystem::path &path, std::string kind, M4 placement) {
    auto owned = std::make_unique<Model>();
    auto &m = *owned;
    cgltf_options options{};
    if (cgltf_parse_file(&options, path.string().c_str(), &m.data) != cgltf_result_success)
      throw std::runtime_error("Cannot load GLB: " + path.string());
    if (cgltf_load_buffers(&options, m.data, path.string().c_str()) != cgltf_result_success ||
        cgltf_validate(m.data) != cgltf_result_success)
      throw std::runtime_error("Invalid GLB: " + path.string());
    m.kind = std::move(kind);
    m.placement = placement;
    m.world.resize(m.data->nodes_count);
    m.visible.resize(m.data->nodes_count);
    for (size_t i = 0; i < m.data->nodes_count; ++i) {
      auto &n = m.data->nodes[i];
      m.positions.push_back({n.translation[0], n.translation[1], n.translation[2]});
      m.scales.push_back({n.scale[0], n.scale[1], n.scale[2]});
      m.rotations.push_back({n.rotation[0], n.rotation[1], n.rotation[2], n.rotation[3]});
    }
    for (size_t i = 0; i < m.data->textures_count; ++i) {
      const auto &t = m.data->textures[i];
      if (t.image && t.image->buffer_view) {
        int w, h, c;
        auto *px = stbi_load_from_memory(cgltf_buffer_view_data(t.image->buffer_view),
                                         int(t.image->buffer_view->size), &w, &h, &c, 4);
        if (!px)
          throw std::runtime_error("GLB texture decode failed");
        unsigned id;
        gl::GenTextures(1, &id);
        gl::BindTexture(GL_TEXTURE_2D, id);
        gl::TexImage2D(GL_TEXTURE_2D, 0, GL_SRGB8_ALPHA8, w, h, 0, GL_RGBA, GL_UNSIGNED_BYTE, px);
        gl::TexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR_MIPMAP_LINEAR);
        gl::TexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
        gl::GenerateMipmap(GL_TEXTURE_2D);
        textures[&t] = id;
        stbi_image_free(px);
      }
    }
    m.weights.resize(m.data->nodes_count);
    for (size_t ni = 0; ni < m.data->nodes_count; ++ni) {
      auto &n = m.data->nodes[ni];
      if (!n.mesh)
        continue;
      const size_t count = n.mesh->primitives_count ? n.mesh->primitives[0].targets_count : 0;
      m.weights[ni].resize(count, 0);
      for (size_t k = 0; k < count; ++k) {
        if (k < n.weights_count)
          m.weights[ni][k] = n.weights[k];
        else if (k < n.mesh->weights_count)
          m.weights[ni][k] = n.mesh->weights[k];
      }
    }
    m.defaults = m.weights;
    for (size_t ni = 0; ni < m.data->nodes_count; ++ni) {
      auto &n = m.data->nodes[ni];
      if (!n.mesh)
        continue;
      for (size_t pi = 0; pi < n.mesh->primitives_count; ++pi) {
        auto &primitive = n.mesh->primitives[pi];
        if (primitive.type != cgltf_primitive_type_triangles)
          throw std::runtime_error("Only triangle primitives supported");
        cgltf_accessor *pos = nullptr;
        cgltf_accessor *normal = nullptr;
        cgltf_accessor *uv = nullptr;
        for (size_t ai = 0; ai < primitive.attributes_count; ++ai) {
          auto &a = primitive.attributes[ai];
          if (a.type == cgltf_attribute_type_position)
            pos = a.data;
          else if (a.type == cgltf_attribute_type_normal)
            normal = a.data;
          else if (a.type == cgltf_attribute_type_texcoord && a.index == 0)
            uv = a.data;
        }
        if (!pos)
          continue;
        std::vector<float> vertices(pos->count * 8);
        Draw d;
        d.model = &m;
        d.node = ni;
        d.min = {1e9f, 1e9f, 1e9f};
        d.max = {-1e9f, -1e9f, -1e9f};
        for (size_t i = 0; i < pos->count; ++i) {
          auto *v = &vertices[i * 8];
          cgltf_accessor_read_float(pos, i, v, 3);
          if (normal)
            cgltf_accessor_read_float(normal, i, v + 3, 3);
          if (uv)
            cgltf_accessor_read_float(uv, i, v + 6, 2);
          // The process-bed rails and the support fronts share z=+/-0.81 m in
          // the source asset. Inset only this static bed mesh by 2 mm near the
          // outer rails, including its bevels, to provide a real depth separation.
          if (n.name && std::string(n.name) == "Rigid_rs_process_bed_root" &&
              std::abs(v[2]) > .60f) {
            v[2] -=
                std::copysign(.002f * std::clamp((std::abs(v[2]) - .60f) / .05f, 0.f, 1.f), v[2]);
            ++rail_repaired_vertices;
          }
          d.min = {std::min(d.min.x, v[0]), std::min(d.min.y, v[1]), std::min(d.min.z, v[2])};
          d.max = {std::max(d.max.x, v[0]), std::max(d.max.y, v[1]), std::max(d.max.z, v[2])};
        }
        std::vector<unsigned> indices(primitive.indices ? primitive.indices->count : pos->count);
        for (size_t i = 0; i < indices.size(); ++i)
          indices[i] = primitive.indices ? unsigned(cgltf_accessor_read_index(primitive.indices, i))
                                         : unsigned(i);
        d.count = int(indices.size());
        if (primitive.material) {
          const auto &mat = *primitive.material;
          const auto &pbr = mat.pbr_metallic_roughness;
          std::copy(pbr.base_color_factor, pbr.base_color_factor + 4, d.base);
          d.metal = pbr.metallic_factor;
          d.rough = pbr.roughness_factor;
          d.transparent = mat.alpha_mode == cgltf_alpha_mode_blend;
          d.double_sided = mat.double_sided;
          d.cutoff = mat.alpha_mode == cgltf_alpha_mode_mask ? mat.alpha_cutoff : .01f;
          d.emission = {mat.emissive_factor[0], mat.emissive_factor[1], mat.emissive_factor[2]};
          if (mat.has_emissive_strength)
            d.emission = d.emission * mat.emissive_strength.emissive_strength;
          if (mat.emissive_texture.texture)
            d.emtex = textures[mat.emissive_texture.texture];
          d.material = mat.name ? mat.name : "";
          if (pbr.base_color_texture.texture)
            d.tex = textures[pbr.base_color_texture.texture];
        }
        // The imported solid support meshes are marked double-sided. At welded joints,
        // their inward caps coincide with the outward shoe face. Cull those inward
        // surfaces consistently, instead of fighting the depth buffer as the view moves.
        const std::string node_name = n.name ? n.name : "";
        const bool support =
            node_name.starts_with("Rigid_rs_") &&
            (node_name.find("module_root") != std::string::npos ||
             node_name.find("support_hinge") != std::string::npos ||
             node_name == "Rigid_rs_machine_root" || node_name == "Rigid_rs_unwinder_root" ||
             node_name == "Rigid_rs_rewinder_root");
        const bool solid_material = d.material == "RS_machine_light" ||
                                    d.material == "RS_machine_paint" ||
                                    d.material == "RS_machine_base" ||
                                    d.material == "RS_machined" || d.material == "RS_silver";
        if (support && solid_material && !d.transparent) {
          d.double_sided = false;
          ++solid_support_draws;
          for (size_t k = 0; k + 2 < indices.size(); k += 3) {
            const float *a = &vertices[indices[k] * 8], *b = &vertices[indices[k + 1] * 8],
                        *c = &vertices[indices[k + 2] * 8];
            const V3 area = cross(V3{b[0] - a[0], b[1] - a[1], b[2] - a[2]},
                                  V3{c[0] - a[0], c[1] - a[1], c[2] - a[2]});
            const V3 normals{a[3] + b[3] + c[3], a[4] + b[4] + c[4], a[5] + b[5] + c[5]};
            if (dot(area, normals) < -1e-8f)
              ++support_winding_errors;
            if (a[4] < -.99f && b[4] < -.99f && c[4] < -.99f && std::abs(a[1] - .386f) < 2e-6f &&
                std::abs(b[1] - a[1]) < 2e-6f && std::abs(c[1] - a[1]) < 2e-6f)
              ++joint_faces;
          }
        }
        d.base_vertices = vertices;
        for (size_t ti = 0; ti < primitive.targets_count; ++ti) {
          std::vector<float> delta(vertices.size(), 0);
          const auto &target = primitive.targets[ti];
          for (size_t ai = 0; ai < target.attributes_count; ++ai) {
            auto &attr = target.attributes[ai];
            int offset = attr.type == cgltf_attribute_type_position ? 0
                         : attr.type == cgltf_attribute_type_normal ? 3
                                                                    : -1;
            if (offset < 0)
              continue;
            for (size_t vi = 0; vi < pos->count; ++vi)
              cgltf_accessor_read_float(attr.data, vi, delta.data() + vi * 8 + offset, 3);
          }
          d.morphs.push_back(std::move(delta));
        }
        if (!d.morphs.empty())
          ++morph_draws;
        gl::GenVertexArrays(1, &d.vao);
        gl::BindVertexArray(d.vao);
        gl::GenBuffers(1, &d.vbo);
        gl::BindBuffer(GL_ARRAY_BUFFER, d.vbo);
        gl::BufferData(GL_ARRAY_BUFFER, vertices.size() * sizeof(float), vertices.data(),
                       GL_STATIC_DRAW);
        gl::GenBuffers(1, &d.ibo);
        gl::BindBuffer(GL_ELEMENT_ARRAY_BUFFER, d.ibo);
        gl::BufferData(GL_ELEMENT_ARRAY_BUFFER, indices.size() * sizeof(unsigned), indices.data(),
                       GL_STATIC_DRAW);
        for (int a = 0; a < 3; ++a) {
          gl::EnableVertexAttribArray(a);
          gl::VertexAttribPointer(
              a, a == 2 ? 2 : 3, GL_FLOAT, GL_FALSE, 8 * sizeof(float),
              reinterpret_cast<void *>(size_t(a == 2 ? 6 : a * 3) * sizeof(float)));
        }
        triangles += d.count / 3;
        draws.push_back(d);
      }
    }
    models.push_back(std::move(owned));
  }
  void reset(Model &m) {
    m.weights = m.defaults;
    for (size_t i = 0; i < m.data->nodes_count; ++i) {
      auto &n = m.data->nodes[i];
      std::copy(m.positions[i].begin(), m.positions[i].end(), n.translation);
      std::copy(m.rotations[i].begin(), m.rotations[i].end(), n.rotation);
      std::copy(m.scales[i].begin(), m.scales[i].end(), n.scale);
    }
  }
  void rotate(Model &m, const std::string &name, int axis, float angle) {
    for (size_t i = 0; i < m.data->nodes_count; ++i) {
      auto &n = m.data->nodes[i];
      if (!n.name || name != n.name)
        continue;
      const auto q = m.rotations[i];
      float r[4]{0, 0, 0, std::cos(angle / 2)};
      r[axis] = std::sin(angle / 2);
      n.rotation[0] = q[3] * r[0] + q[0] * r[3] + q[1] * r[2] - q[2] * r[1];
      n.rotation[1] = q[3] * r[1] - q[0] * r[2] + q[1] * r[3] + q[2] * r[0];
      n.rotation[2] = q[3] * r[2] + q[0] * r[1] - q[1] * r[0] + q[2] * r[3];
      n.rotation[3] = q[3] * r[3] - q[0] * r[0] - q[1] * r[1] - q[2] * r[2];
    }
  }
  // Showcase samples glTF channels independently of simulation time. STEP and
  // LINEAR (including shortest-arc quaternion interpolation) cover this asset.
  void sample(Model &m, float time) {
    if (!m.data->animations_count)
      return;
    auto &anim = m.data->animations[0];
    for (size_t i = 0; i < anim.channels_count; ++i) {
      auto &ch = anim.channels[i];
      auto &s = *ch.sampler;
      if (s.interpolation == cgltf_interpolation_type_cubic_spline)
        throw std::runtime_error("Unsupported CUBICSPLINE asset");
      size_t a = 0;
      float t0 = 0, t1 = 0;
      while (a + 1 < s.input->count) {
        cgltf_accessor_read_float(s.input, a + 1, &t1, 1);
        if (t1 > time)
          break;
        ++a;
      }
      size_t b = std::min(a + 1, s.input->count - 1);
      cgltf_accessor_read_float(s.input, a, &t0, 1);
      cgltf_accessor_read_float(s.input, b, &t1, 1);
      float u = s.interpolation == cgltf_interpolation_type_step
                    ? 0
                    : std::clamp((time - t0) / std::max(1e-6f, t1 - t0), 0.f, 1.f);
      if (ch.target_path == cgltf_animation_path_type_weights) {
        auto &w = m.weights[size_t(ch.target_node - m.data->nodes)];
        for (size_t k = 0; k < w.size(); ++k) {
          float av = 0, bv = 0;
          cgltf_accessor_read_float(s.output, a * w.size() + k, &av, 1);
          cgltf_accessor_read_float(s.output, b * w.size() + k, &bv, 1);
          w[k] = (1 - u) * av + u * bv;
        }
        continue;
      }
      float av[4]{}, bv[4]{}, v[4]{};
      int count = ch.target_path == cgltf_animation_path_type_rotation ? 4 : 3;
      cgltf_accessor_read_float(s.output, a, av, count);
      cgltf_accessor_read_float(s.output, b, bv, count);
      if (count == 4) {
        float d = 0;
        for (int k = 0; k < 4; ++k)
          d += av[k] * bv[k];
        if (d < 0) {
          for (float &f : bv)
            f = -f;
          d = -d;
        }
        if (d < .9995f) {
          float th = std::acos(std::clamp(d, -1.f, 1.f));
          for (int k = 0; k < 4; ++k)
            v[k] = (std::sin((1 - u) * th) * av[k] + std::sin(u * th) * bv[k]) / std::sin(th);
        } else {
          float len = 0;
          for (int k = 0; k < 4; ++k) {
            v[k] = (1 - u) * av[k] + u * bv[k];
            len += v[k] * v[k];
          }
          for (float &f : v)
            f /= std::sqrt(std::max(len, 1e-9f));
        }
      } else
        for (int k = 0; k < 3; ++k)
          v[k] = (1 - u) * av[k] + u * bv[k];
      if (ch.target_path == cgltf_animation_path_type_rotation)
        std::copy(v, v + 4, ch.target_node->rotation);
      else if (ch.target_path == cgltf_animation_path_type_translation)
        std::copy(v, v + 3, ch.target_node->translation);
      else if (ch.target_path == cgltf_animation_path_type_scale)
        std::copy(v, v + 3, ch.target_node->scale);
    }
  }
  void compute(Model &m, size_t index, const M4 &parent, bool shown) {
    auto &n = m.data->nodes[index];
    M4 local;
    cgltf_node_transform_local(&n, local.m);
    m.world[index] = mul(parent, local);
    const std::string name = n.name ? n.name : "";
    bool on = shown && !hidden.count(name);
    if (n.extras.data) {
      const auto e = json::parse(n.extras.data, nullptr, false);
      if (e.is_object() && e.contains("slx_default_visible") &&
          !e["slx_default_visible"].get<bool>())
        on = false;
    }
    // These are the two explicit optional roots activated by the chosen preset.
    if (m.overrides.count(name))
      on = shown && m.overrides.at(name) && !hidden.count(name);
    m.visible[index] = on;
    for (size_t i = 0; i < n.children_count; ++i)
      compute(m, size_t(n.children[i] - m.data->nodes), m.world[index], on);
  }
};
Renderer::Renderer(const std::filesystem::path &root) : p(std::make_unique<RendererImpl>()) {
  const auto start = std::chrono::steady_clock::now();
  p->shader = shader_program(vertex, fragment);
  p->load(root / "scene/SLX_ReelSync_Runtime.glb", "runtime", M4::identity());
  p->gaugeOverlay();
  p->cameras = read_json(root / "config/camera_presets.json")["presets"];
  p->clip_tracking = read_json(root / "config/kinematic_samples.json")["states"];
  const auto contract = read_json(root / "config/scene_contract.json");
  std::set<std::string> names;
  for (size_t i = 0; i < p->models[0]->data->nodes_count; ++i)
    if (p->models[0]->data->nodes[i].name)
      names.insert(p->models[0]->data->nodes[i].name);
  for (const auto *n :
       {"rs_unwinder_spindle", "rs_rewinder_spindle", "RS_WebFilm", "rs_dancer_carriage"})
    if (!names.count(n))
      throw std::runtime_error(std::string("Missing ReelSync scene node: ") + n);
  focus(0);
  p->shadow_shader = shader_program(depthVertex, depthFragment);
  p->light_vp = mul(ortho(11), look({-9, 20, 13}, {0, 1.4f, 0}, {0, 1, 0}));
  gl::GenFramebuffers(1, &p->shadow_fbo);
  gl::GenTextures(1, &p->shadow_tex);
  gl::BindTexture(GL_TEXTURE_2D, p->shadow_tex);
  gl::TexImage2D(GL_TEXTURE_2D, 0, GL_DEPTH_COMPONENT24, 2048, 2048, 0, GL_DEPTH_COMPONENT,
                 GL_FLOAT, nullptr);
  gl::TexParameteri(GL_TEXTURE_2D, GL_TEXTURE_COMPARE_MODE, GL_COMPARE_REF_TO_TEXTURE);
  gl::TexParameteri(GL_TEXTURE_2D, GL_TEXTURE_COMPARE_FUNC, GL_LEQUAL);
  gl::TexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
  gl::TexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
  gl::TexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
  gl::TexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
  gl::BindFramebuffer(GL_FRAMEBUFFER, p->shadow_fbo);
  gl::FramebufferTexture2D(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, GL_TEXTURE_2D, p->shadow_tex, 0);
  gl::DrawBuffer(GL_NONE);
  gl::ReadBuffer(GL_NONE);
  if (gl::CheckFramebufferStatus(GL_FRAMEBUFFER) != GL_FRAMEBUFFER_COMPLETE)
    throw std::runtime_error("Shadow framebuffer unavailable");
  gl::BindFramebuffer(GL_FRAMEBUFFER, 0);
  gl::GenFramebuffers(1, &p->fbo);
  gl::GenFramebuffers(1, &p->ms_fbo);
  gl::GenTextures(1, &p->target);
  gl::GenRenderbuffers(1, &p->depth);
  gl::GenRenderbuffers(1, &p->ms_color);
  p->load_ms =
      std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - start).count();
}
Renderer::~Renderer() {
  for (auto &d : p->draws) {
    gl::DeleteVertexArrays(1, &d.vao);
    gl::DeleteBuffers(1, &d.vbo);
    gl::DeleteBuffers(1, &d.ibo);
  }
  for (auto [key, id] : p->textures)
    gl::DeleteTextures(1, &id);
  for (unsigned id : p->product_images)
    gl::DeleteTextures(1, &id);
  gl::DeleteTextures(1, &p->target);
  gl::DeleteRenderbuffers(1, &p->depth);
  gl::DeleteRenderbuffers(1, &p->ms_color);
  gl::DeleteFramebuffers(1, &p->fbo);
  gl::DeleteFramebuffers(1, &p->ms_fbo);
  gl::DeleteProgram(p->shader);
  gl::DeleteProgram(p->shadow_shader);
  gl::DeleteTextures(1, &p->shadow_tex);
  gl::DeleteFramebuffers(1, &p->shadow_fbo);
}
ImageInfo Renderer::image_info(const std::filesystem::path &path) {
  int w = 0, h = 0, c = 0;
  auto *px = stbi_load(path.string().c_str(), &w, &h, &c, 4);
  if (!px)
    throw std::runtime_error("Product image unavailable");
  int xmin = w, ymin = h, xmax = 0, ymax = 0;
  for (int y = 0; y < h; ++y)
    for (int x = 0; x < w; ++x)
      if (px[(y * w + x) * 4 + 3] > 8) {
        xmin = std::min(xmin, x);
        xmax = std::max(xmax, x + 1);
        ymin = std::min(ymin, y);
        ymax = std::max(ymax, y + 1);
      }
  if (xmax <= xmin || ymax <= ymin) {
    xmin = ymin = 0;
    xmax = w;
    ymax = h;
  }
  unsigned id = 0;
  gl::GenTextures(1, &id);
  gl::BindTexture(GL_TEXTURE_2D, id);
  gl::TexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, w, h, 0, GL_RGBA, GL_UNSIGNED_BYTE, px);
  gl::TexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
  gl::TexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
  stbi_image_free(px);
  p->product_images.push_back(id);
  return {id,
          float(xmax - xmin) / float(ymax - ymin),
          {float(xmin) / w, float(ymin) / h},
          {float(xmax) / w, float(ymax) / h}};
}
unsigned Renderer::image_file(const std::filesystem::path &path) {
  return image_info(path).texture;
}
void Renderer::window_icon(GLFWwindow *window, const std::filesystem::path &path) {
  int w = 0, h = 0, c = 0;
  auto *px = stbi_load(path.string().c_str(), &w, &h, &c, 4);
  if (!px)
    throw std::runtime_error("Application icon unavailable");
  GLFWimage icon{w, h, px};
  glfwSetWindowIcon(window, 1, &icon);
  stbi_image_free(px);
}
unsigned Renderer::render(int width, int height, const json &state, bool cutaway, bool showcase,
                          double clip_s, const json &mechanism_pose) {
  const auto start = std::chrono::steady_clock::now();
  width = std::clamp(width, 64, 2560);
  height = std::clamp(height, 64, 1600);
  p->state = state;
  p->cutaway = cutaway;
  if (p->fit_requested) {
    float yaw = p->desired_yaw, pitch = p->desired_pitch;
    V3 f = norm(V3{-std::sin(yaw) * std::cos(pitch), -std::sin(pitch),
                   -std::cos(yaw) * std::cos(pitch)}),
       right = norm(cross(f, {0, 1, 0})), up = cross(right, f);
    float d = 1;
    for (int k = 0; k < 8; ++k) {
      V3 q{(k & 1 ? 1.f : -1.f) * 6.1f, (k & 2 ? 1.f : -1.f) * 1.65f, (k & 4 ? 1.f : -1.f) * 1.4f};
      d = std::max(d, std::max(std::abs(dot(q, right)) / std::tan(p->hfov / 2),
                               std::abs(dot(q, up)) / std::tan(p->hfov / 2) * width / height) -
                          dot(q, f));
    }
    p->desired_radius = d * 1.15f;
    p->radius = p->desired_radius;
    p->center = p->desired_center;
    p->yaw = p->desired_yaw;
    p->pitch = p->desired_pitch;
    p->fit_requested = false;
  }
  if (width != p->width || height != p->height) {
    p->width = width;
    p->height = height;
    gl::BindTexture(GL_TEXTURE_2D, p->target);
    gl::TexImage2D(GL_TEXTURE_2D, 0, GL_RGBA8, width, height, 0, GL_RGBA, GL_UNSIGNED_BYTE,
                   nullptr);
    gl::TexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    gl::TexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    gl::BindFramebuffer(GL_FRAMEBUFFER, p->fbo);
    gl::FramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, p->target, 0);
    gl::BindRenderbuffer(GL_RENDERBUFFER, p->depth);
    gl::RenderbufferStorageMultisample(GL_RENDERBUFFER, 4, GL_DEPTH_COMPONENT32F, width, height);
    gl::BindRenderbuffer(GL_RENDERBUFFER, p->ms_color);
    gl::RenderbufferStorageMultisample(GL_RENDERBUFFER, 4, GL_RGBA8, width, height);
    gl::BindFramebuffer(GL_FRAMEBUFFER, p->ms_fbo);
    gl::FramebufferRenderbuffer(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_RENDERBUFFER, p->ms_color);
    gl::FramebufferRenderbuffer(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, GL_RENDERBUFFER, p->depth);
  }
  p->instruments(state, showcase);
  p->hidden.clear();
  if (p->hide_guards)
    for (const auto *n : {"rs_front_guards_root", "rs_end_guard_-1", "rs_end_guard_1"})
      p->hidden.insert(n);
  if (p->hide_hall)
    for (const auto *n :
         {"rs_hall_rear_root", "rs_hall_west_root", "rs_hall_east_root", "rs_hall_structure_root",
          "rs_hall_services_root", "rs_spares_and_workbench_root"})
      p->hidden.insert(n);

  if (cutaway)
    for (const char *n : {"rs_hall_rear_root", "rs_hall_west_root", "rs_hall_east_root",
                          "rs_front_guards_root", "rs_end_guard_-1", "rs_end_guard_1"})
      p->hidden.insert(n);
  for (auto &owned : p->models) {
    auto &m = *owned;
    p->reset(m);
    if (showcase)
      p->sample(m, float(std::clamp(clip_s, 0., 60.)));
    else {
      const char *rotors[] = {"rs_unwinder_spindle", "rs_entry_roller",    "rs_loadcell_roller",
                              "rs_drive_roller",     "rs_dancer_guide_in", "rs_dancer_roller",
                              "rs_dancer_guide_out", "rs_exit_roller",     "rs_rewinder_spindle"};
      for (int k = 0; k < 9; ++k)
        p->rotate(m, rotors[k], 2, -state.at("phase")[k].get<float>());
      p->rotate(m, "rs_nip_roller", 2, -state.at("distance_m").get<float>() / .08f);
      for (size_t i = 0; i < m.data->nodes_count; ++i) {
        auto &n = m.data->nodes[i];
        const std::string name = n.name ? n.name : "";
        if (name == "rs_dancer_carriage")
          n.translation[1] = state.at("dancer_z_m").get<float>();
        if (name == "RS_UnwinderFilmRoll" && !m.weights[i].empty())
          m.weights[i][0] = (.62f - state.at("ru").get<float>()) / .42f;
        if (name == "RS_RewinderFilmRoll" && !m.weights[i].empty())
          m.weights[i][0] = (state.at("rr").get<float>() - .25f) / .37f;
      }
      if (state.at("broken").get<bool>())
        p->hidden.insert("RS_WebFilm");
      if (state.at("maintenance").get<bool>()) {
        p->rotate(m, "rs_rewind_guard_hinge", 1, 1.22173f);
        p->rotate(m, "rs_unwind_guard_hinge", 1, -1.22173f);
        p->rotate(m, "rs_nip_arm", 2, -.698132f);
        p->rotate(m, "rs_unwinder_front_support_hinge", 0, 1.22173f);
        p->rotate(m, "rs_rewinder_front_support_hinge", 0, 1.22173f);
      }
    }
    for (size_t i = 0; i < m.data->nodes_count; ++i)
      if (!m.data->nodes[i].parent)
        p->compute(m, i, m.placement, true);
  }
  // Live route is generated by the core; presentation alone uses baked morph weights.
  for (auto &d : p->draws) {
    if (d.morphs.empty())
      continue;
    auto &m = *d.model;
    const std::string name = m.data->nodes[d.node].name;
    std::vector<float> v = d.base_vertices;
    if (name == "RS_WebFilm" && !showcase) {
      const auto &path = state.at("path");
      const size_t n = path.size();
      v.assign(n * 2 * 8, 0);
      const float width = state.at("width_m").get<float>();
      for (size_t i = 0; i < n; ++i) {
        const auto &a = path[i ? i - 1 : i];
        const auto &b = path[std::min(i + 1, n - 1)];
        V3 normal = norm(
            V3{b[1].get<float>() - a[1].get<float>(), a[0].get<float>() - b[0].get<float>(), 0});
        for (size_t k = 0; k < 2; ++k) {
          auto *q = v.data() + (i * 2 + k) * 8;
          q[0] = path[i][0];
          q[1] = path[i][1];
          q[2] = (k ? .5f : -.5f) * width;
          q[3] = normal.x;
          q[4] = normal.y;
          q[5] = 0;
          const auto &tracking = state.at("web_tracking");
          const double origin = tracking.at("origin_m");
          const double u = tracking.at("upstream_route_m"), r = tracking.at("downstream_route_m");
          const double s = path[i][3];
          const double material =
              origin + (s <= u ? tracking.at("upstream_rest_m").get<double>() * s / u
                               : tracking.at("upstream_rest_m").get<double>() +
                                     tracking.at("downstream_rest_m").get<double>() * (s - u) / r);
          q[6] = float(material - std::floor(origin / p->marker_pitch) * p->marker_pitch);
          q[7] = float(k);
        }
      }
      std::vector<unsigned> indices;
      for (unsigned i = 0; i + 1 < n; ++i) {
        unsigned a = 2 * i;
        for (unsigned k : {a, a + 1, a + 3, a, a + 3, a + 2})
          indices.push_back(k);
      }
      gl::BindVertexArray(d.vao);
      gl::BindBuffer(GL_ELEMENT_ARRAY_BUFFER, d.ibo);
      gl::BufferData(GL_ELEMENT_ARRAY_BUFFER, indices.size() * sizeof(unsigned), indices.data(),
                     GL_DYNAMIC_DRAW);
      d.count = int(indices.size());
      p->live_path_vertices = int(n * 2);
    } else {
      const auto &weights = m.weights[d.node];
      for (size_t k = 0; k < d.morphs.size(); ++k)
        for (size_t j = 0; j < v.size(); ++j)
          v[j] += weights[k] * d.morphs[k][j];
      // Restore indices when leaving live mode; imported web has different topology.
      auto &prim = m.data->nodes[d.node].mesh->primitives[0];
      if (name == "RS_WebFilm") {
        double feed = p->clip_tracking.back().at("feed_length_m");
        for (size_t i = 1; i < p->clip_tracking.size(); ++i) {
          const auto &a = p->clip_tracking[i - 1], &b = p->clip_tracking[i];
          if (clip_s <= b.at("seconds").get<double>()) {
            const double alpha =
                std::clamp((clip_s - a.at("seconds").get<double>()) /
                               (b.at("seconds").get<double>() - a.at("seconds").get<double>()),
                           0., 1.);
            feed = a.at("feed_length_m").get<double>() * (1 - alpha) +
                   b.at("feed_length_m").get<double>() * alpha;
            break;
          }
        }
        feed = std::fmod(feed, double(p->marker_pitch));
        for (size_t i = 6; i < v.size(); i += 8)
          v[i] -= float(feed);
        std::vector<unsigned> ix(prim.indices->count);
        for (size_t i = 0; i < ix.size(); ++i)
          ix[i] = unsigned(cgltf_accessor_read_index(prim.indices, i));
        gl::BindVertexArray(d.vao);
        gl::BindBuffer(GL_ELEMENT_ARRAY_BUFFER, d.ibo);
        gl::BufferData(GL_ELEMENT_ARRAY_BUFFER, ix.size() * sizeof(unsigned), ix.data(),
                       GL_DYNAMIC_DRAW);
        d.count = int(ix.size());
      }
      if (!showcase && (name == "RS_UnwinderFilmRoll" || name == "RS_RewinderFilmRoll"))
        for (size_t j = 2; j < v.size(); j += 8)
          v[j] *= state.at("width_m").get<float>() / .8f;
    }
    d.min = {1e9f, 1e9f, 1e9f};
    d.max = {-1e9f, -1e9f, -1e9f};
    for (size_t i = 0; i < v.size(); i += 8) {
      d.min = {std::min(d.min.x, v[i]), std::min(d.min.y, v[i + 1]), std::min(d.min.z, v[i + 2])};
      d.max = {std::max(d.max.x, v[i]), std::max(d.max.y, v[i + 1]), std::max(d.max.z, v[i + 2])};
    }
    gl::BindBuffer(GL_ARRAY_BUFFER, d.vbo);
    gl::BufferData(GL_ARRAY_BUFFER, v.size() * sizeof(float), v.data(), GL_DYNAMIC_DRAW);
  }
  p->eye = p->center + V3{std::sin(p->yaw) * std::cos(p->pitch), std::sin(p->pitch),
                          std::cos(p->yaw) * std::cos(p->pitch)} *
                           p->radius;
  p->eye.y = std::max(.2f, p->eye.y);
  p->near_plane = std::clamp(p->radius * .0125f, .08f, .45f);
  p->far_plane = std::max(60.f, p->radius + 40);
  p->vp = mul(perspective(float(width) / height, p->hfov, p->near_plane, p->far_plane),
              look(p->eye, p->center, p->up));
  p->shadow_draws = 0;
  if (p->shadows) {
    gl::BindFramebuffer(GL_FRAMEBUFFER, p->shadow_fbo);
    gl::Viewport(0, 0, 2048, 2048);
    gl::Disable(GL_SCISSOR_TEST);
    gl::Disable(GL_BLEND);
    gl::Enable(GL_DEPTH_TEST);
    gl::DepthMask(GL_TRUE);
    gl::Clear(GL_DEPTH_BUFFER_BIT);
    gl::Enable(GL_POLYGON_OFFSET_FILL);
    gl::PolygonOffset(2, 4);
    gl::UseProgram(p->shadow_shader);
    gl::Disable(GL_CULL_FACE);
    gl::UniformMatrix4fv(gl::GetUniformLocation(p->shadow_shader, "lightVP"), 1, GL_FALSE,
                         p->light_vp.m);
    for (const auto &d : p->draws)
      if (d.model->visible[d.node] && !d.transparent) {
        gl::UniformMatrix4fv(gl::GetUniformLocation(p->shadow_shader, "model"), 1, GL_FALSE,
                             d.model->world[d.node].m);
        gl::BindVertexArray(d.vao);
        gl::DrawElements(GL_TRIANGLES, d.count, GL_UNSIGNED_INT, nullptr);
        ++p->shadow_draws;
      }
    gl::Disable(GL_POLYGON_OFFSET_FILL);
  }
  const float daylight = 1.f;
  gl::BindFramebuffer(GL_FRAMEBUFFER, p->ms_fbo);
  gl::Viewport(0, 0, width, height);
  gl::Disable(GL_SCISSOR_TEST);
  gl::Enable(GL_DEPTH_TEST);
  gl::DepthFunc(GL_LESS);
  gl::Disable(GL_BLEND);
  gl::DepthMask(GL_TRUE);
  gl::ClearColor(.18f, .23f, .30f, 1);
  gl::Clear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
  gl::UseProgram(p->shader);
  gl::UniformMatrix4fv(gl::GetUniformLocation(p->shader, "vp"), 1, GL_FALSE, p->vp.m);
  gl::Uniform3f(gl::GetUniformLocation(p->shader, "eye"), p->eye.x, p->eye.y, p->eye.z);
  gl::Uniform1f(gl::GetUniformLocation(p->shader, "daylight"), daylight);
  gl::Uniform1f(gl::GetUniformLocation(p->shader, "exposure"), p->exposure);
  gl::Uniform1i(gl::GetUniformLocation(p->shader, "tex"), 0);
  gl::Uniform1i(gl::GetUniformLocation(p->shader, "emTex"), 1);
  gl::UniformMatrix4fv(gl::GetUniformLocation(p->shader, "lightVP"), 1, GL_FALSE, p->light_vp.m);
  gl::Uniform1f(gl::GetUniformLocation(p->shader, "keyLight"), p->key_light);
  gl::Uniform1i(gl::GetUniformLocation(p->shader, "shadowEnabled"), p->shadows ? 1 : 0);
  gl::Uniform1i(gl::GetUniformLocation(p->shader, "shadowTex"), 2);
  gl::ActiveTexture(GL_TEXTURE2);
  gl::BindTexture(GL_TEXTURE_2D, p->shadow_tex);

  gl::ActiveTexture(GL_TEXTURE0);
  std::vector<const RendererImpl::Draw *> ordered;
  for (const auto &d : p->draws)
    if (d.model->visible[d.node])
      ordered.push_back(&d);
  std::stable_sort(ordered.begin(), ordered.end(), [&](auto *a, auto *b) {
    if (a->transparent != b->transparent)
      return !a->transparent;
    if (!a->transparent)
      return false;
    const auto av = point(a->model->world[a->node], (a->min + a->max) * .5f) - p->eye,
               bv = point(b->model->world[b->node], (b->min + b->max) * .5f) - p->eye;
    return dot(av, av) > dot(bv, bv);
  });
  const auto diagnostic = state.value("alarm_status", json::object());
  const auto attention_parts = diagnostic.value("attention_parts", std::vector<std::string>{});
  p->attention_draws = 0;
  for (const auto *d : ordered) {
    if (d->transparent) {
      gl::Enable(GL_BLEND);
      gl::BlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
      gl::DepthMask(GL_FALSE);
    }
    if (d->double_sided)
      gl::Disable(GL_CULL_FACE);
    else {
      gl::Enable(GL_CULL_FACE);
      gl::CullFace(GL_BACK);
    }
    float base[4];
    std::copy(d->base, d->base + 4, base);
    float metal = d->metal, rough = d->rough;
    if (p->style == 1) {
      static const std::map<std::string, unsigned> colors = {
          {"RS_machine_paint", 0x385169},
          {"RS_machine_base", 0x233649},
          {"RS_machine_light", 0xB4C1CC},
          {"RS_Guard_Frame", 0x293D50},
          {"RS_roller_aluminium", 0x9CACBA},
          {"RS_machined", 0x7D8E9F},
          {"RS_PET_Coil", 0xB47D43},
          {"RS_PET_Layer_Edges", 0x8C6238},
          {"RS_PET_Web", 0xD9AC68},
          {"RS_Hall_concrete_satin", 0x9DA8B0},
          {"RS_Hall_workzone_epoxy", 0x8C9DA7},
          {"RS_Hall_light_sandwich_panel", 0x9EAAB5},
          {"RS_motor_blue", 0x294D70}};
      auto color = colors.find(d->material);
      if (color != colors.end()) {
        auto rgb = linearRGB(color->second);
        base[0] = rgb.x;
        base[1] = rgb.y;
        base[2] = rgb.z;
      }
      if (d->material == "RS_PET_Coil" || d->material == "RS_PET_Layer_Edges") {
        metal = .08f;
        rough = .45f;
      }
      if (d->material == "RS_PET_Web") {
        metal = .06f;
        rough = .4f;
        base[3] = 1;
      }
    }
    bool selected = false;
    const std::string selection = state.value("selected", std::string());
    for (auto *n = &d->model->data->nodes[d->node]; n; n = n->parent)
      if (n->name && selection == n->name)
        selected = true;
    if (d->material == "RS_SiliconLogiX_original_logo")
      selected = false;
    bool affected = false;
    if (!showcase)
      for (auto *n = &d->model->data->nodes[d->node]; n; n = n->parent)
        if (n->name)
          for (const auto &part : attention_parts)
            affected |= slx::reelsync::partMatches(part, n->name);
    if (affected)
      ++p->attention_draws;
    gl::Uniform1i(gl::GetUniformLocation(p->shader, "attention"),
                  affected ? (diagnostic.value("active", 0u) ? 2 : 1) : 0);
    const bool layered = d->material == "RS_PET_Coil" || d->material == "RS_PET_Layer_Edges";
    V3 axis = point(d->model->world[d->node], {});
    gl::Uniform3f(gl::GetUniformLocation(p->shader, "axisCenter"), axis.x, axis.y, axis.z);
    gl::Uniform1i(gl::GetUniformLocation(p->shader, "layered"), layered ? 1 : 0);
    gl::Uniform1i(gl::GetUniformLocation(p->shader, "webSurface"),
                  p->web_markers && d->material == "RS_PET_Web" ? 1 : 0);
    gl::Uniform1f(gl::GetUniformLocation(p->shader, "markerPitch"), p->marker_pitch);
    gl::Uniform1i(gl::GetUniformLocation(p->shader, "selected"), selected ? 1 : 0);
    const bool hmi = d->material == "RS_HMI_static_display",
               gauge = d->material == "RS_live_force_display";
    const unsigned surface_texture = hmi ? p->live_hmi : gauge ? p->live_gauge : d->tex;
    gl::Uniform1i(gl::GetUniformLocation(p->shader, "instrumentSurface"), hmi || gauge ? 1 : 0);
    V3 emission = d->emission;
    if (!showcase && d->material == "RS_led_green" &&
        state.at("operating_state") != SLX_RS_RUNNING) {
      emission = emission * .03f;
      for (int k = 0; k < 3; ++k)
        base[k] *= .25f;
    }
    gl::UniformMatrix4fv(gl::GetUniformLocation(p->shader, "model"), 1, GL_FALSE,
                         d->model->world[d->node].m);
    gl::Uniform4fv(gl::GetUniformLocation(p->shader, "base"), 1, base);
    gl::Uniform3f(gl::GetUniformLocation(p->shader, "emission"), emission.x, emission.y,
                  emission.z);
    gl::Uniform1f(gl::GetUniformLocation(p->shader, "metallic"), metal);
    gl::Uniform1f(gl::GetUniformLocation(p->shader, "roughness"), rough);
    gl::Uniform1f(gl::GetUniformLocation(p->shader, "cutoff"), d->cutoff);
    gl::Uniform1i(gl::GetUniformLocation(p->shader, "hasTex"), surface_texture ? 1 : 0);
    gl::Uniform1i(gl::GetUniformLocation(p->shader, "hasEmTex"), d->emtex ? 1 : 0);
    gl::ActiveTexture(GL_TEXTURE1);
    gl::BindTexture(GL_TEXTURE_2D, d->emtex);
    gl::ActiveTexture(GL_TEXTURE0);
    gl::BindTexture(GL_TEXTURE_2D, surface_texture);
    gl::BindVertexArray(d->vao);
    gl::DrawElements(GL_TRIANGLES, d->count, GL_UNSIGNED_INT, nullptr);
  }
  gl::Disable(GL_CULL_FACE);
  gl::Disable(GL_BLEND);
  gl::DepthMask(GL_TRUE);
  gl::BindVertexArray(0);
  gl::BindFramebuffer(GL_READ_FRAMEBUFFER, p->ms_fbo);
  gl::BindFramebuffer(GL_DRAW_FRAMEBUFFER, p->fbo);
  gl::BlitFramebuffer(0, 0, width, height, 0, 0, width, height, GL_COLOR_BUFFER_BIT, GL_NEAREST);
  gl::BindFramebuffer(GL_FRAMEBUFFER, 0);
  p->render_ms =
      std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - start).count();
  return p->target;
}
void Renderer::exposure(float value) {
  p->exposure = std::clamp(value, .45f, 1.6f);
}
void Renderer::appearance(int style, bool guards, bool hall, bool shadows, float light) {
  p->style = std::clamp(style, 0, 1);
  p->hide_guards = guards;
  p->hide_hall = hall;
  p->shadows = shadows;
  p->key_light = std::clamp(light, .35f, 1.8f);
}
void Renderer::orbit(float dx, float dy) {
  p->desired_yaw -= dx * .006f;
  p->desired_pitch = std::clamp(p->desired_pitch + dy * .006f, -.05f, 1.45f);
  p->up = {0, 1, 0};
  p->fit_requested = false;
}
void Renderer::zoom(float amount) {
  p->desired_radius = std::clamp(p->desired_radius * std::exp(-amount * .12f), .65f, 70.f);
  p->fit_requested = false;
}
void Renderer::pan(float dx, float dy) {
  auto f = norm(p->center - p->eye), right = norm(cross(f, {0, 1, 0})), up = cross(right, f);
  float scale = 2 * p->radius * std::tan(p->hfov / 2) / std::max(64, p->width);
  p->desired_center = p->desired_center + right * (-dx * scale) + up * (dy * scale);
  p->fit_requested = false;
}
void Renderer::update(double dt) {
  float a = 1 - std::exp(-14 * float(std::clamp(dt, 0., .1)));
  p->center = p->center + (p->desired_center - p->center) * a;
  p->yaw += std::remainder(p->desired_yaw - p->yaw, 6.2831853f) * a;
  p->pitch += (p->desired_pitch - p->pitch) * a;
  p->radius += (p->desired_radius - p->radius) * a;
}
void Renderer::focus(int preset) {
  static const char *names[] = {
      "Camera_Hero", "Camera_Overview", "Camera_Dancer",   "Camera_Rewinder", "Camera_Unwinder",
      "Camera_HMI",  "Camera_Cabinet",  "Camera_Services", "Camera_Line",     "Camera_Branding"};
  preset = std::clamp(preset, 0, 9);
  p->up = {0, 1, 0};
  if (preset == 0) {
    p->desired_center = {0, 1.6f, 0};
    p->desired_yaw = .52f;
    p->desired_pitch = .31f;
    p->hfov = 1.02f;
    p->fit_requested = true;
    return;
  }
  for (const auto &c : p->cameras)
    if (c["name"] == names[preset]) {
      auto v = c["position_gltf_m"].get<std::array<float, 3>>(),
           t = c["target_gltf_m"].get<std::array<float, 3>>();
      p->desired_center = {t[0], t[1], t[2]};
      V3 d = V3{v[0], v[1], v[2]} - p->desired_center;
      p->desired_radius = std::sqrt(dot(d, d));
      p->desired_yaw = std::atan2(d.x, d.z);
      p->desired_pitch = std::asin(d.y / p->desired_radius);
      p->hfov = c["horizontal_fov_rad"];
      p->fit_requested = false;
      return;
    }
}
void Renderer::frame(const std::string &semantic) {
  if (semantic.empty()) {
    focus(0);
    return;
  }
  V3 lo{1e9f, 1e9f, 1e9f}, hi{-1e9f, -1e9f, -1e9f};
  bool found = false;
  for (const auto &d : p->draws) {
    bool belongs = false;
    for (auto *n = &d.model->data->nodes[d.node]; n; n = n->parent)
      if (n->name && semantic == n->name)
        belongs = true;
    if (!belongs)
      continue;
    found = true;
    for (int k = 0; k < 8; ++k) {
      auto v = point(d.model->world[d.node], {k & 1 ? d.max.x : d.min.x, k & 2 ? d.max.y : d.min.y,
                                              k & 4 ? d.max.z : d.min.z});
      lo = {std::min(lo.x, v.x), std::min(lo.y, v.y), std::min(lo.z, v.z)};
      hi = {std::max(hi.x, v.x), std::max(hi.y, v.y), std::max(hi.z, v.z)};
    }
  }
  if (found) {
    p->desired_center = (lo + hi) * .5f;
    V3 extent = (hi - lo) * .5f;
    V3 f = norm(V3{-std::sin(p->desired_yaw) * std::cos(p->desired_pitch),
                   -std::sin(p->desired_pitch),
                   -std::cos(p->desired_yaw) * std::cos(p->desired_pitch)}),
       right = norm(cross(f, {0, 1, 0})), up = cross(right, f);
    float radius = .9f, aspect = float(std::max(64, p->width)) / std::max(64, p->height);
    for (int k = 0; k < 8; ++k) {
      V3 q{(k & 1 ? 1.f : -1.f) * extent.x, (k & 2 ? 1.f : -1.f) * extent.y,
           (k & 4 ? 1.f : -1.f) * extent.z};
      radius = std::max(radius, std::max(std::abs(dot(q, right)) / std::tan(p->hfov / 2),
                                         std::abs(dot(q, up)) / std::tan(p->hfov / 2) * aspect) -
                                    dot(q, f));
    }
    p->desired_radius = radius * 1.22f;
    p->fit_requested = false;
  }
}
json Renderer::camera_pose() const {
  return {{"center", {p->desired_center.x, p->desired_center.y, p->desired_center.z}},
          {"yaw", p->desired_yaw},
          {"pitch", p->desired_pitch},
          {"radius", p->desired_radius},
          {"hfov", p->hfov}};
}
void Renderer::camera_pose(const json &v, bool immediate) {
  auto c = v.at("center").get<std::array<float, 3>>();
  float y = v.at("yaw"), pitch = v.at("pitch"), r = v.at("radius"), fov = v.at("hfov");
  for (float x : {c[0], c[1], c[2], y, pitch, r, fov})
    if (!std::isfinite(x))
      throw std::runtime_error("Posa camera non valida");
  if (std::abs(c[0]) > 30 || std::abs(c[1]) > 20 || std::abs(c[2]) > 30 || r < .65f || r > 70 ||
      pitch < -.3f || pitch > 1.56f || fov < .3f || fov > 1.8f)
    throw std::runtime_error("Posa camera fuori campo");
  p->desired_center = {c[0], c[1], c[2]};
  p->desired_yaw = y;
  p->desired_pitch = pitch;
  p->desired_radius = r;
  p->hfov = fov;
  p->up = {0, 1, 0};
  p->fit_requested = false;
  if (immediate) {
    p->center = p->desired_center;
    p->yaw = p->desired_yaw;
    p->pitch = p->desired_pitch;
    p->radius = p->desired_radius;
  }
}
std::string Renderer::pick(float x, float y) const {
  const auto f = norm(p->center - p->eye), right = norm(cross(f, p->up)), up = cross(right, f);
  const auto ray = norm(f + right * ((2 * x - 1) * std::tan(p->hfov / 2)) +
                        up * ((1 - 2 * y) * std::tan(p->hfov / 2) * p->height / p->width));
  float closest = 1e9f;
  std::string selected;
  for (const auto &d : p->draws) {
    if (!d.model->visible[d.node])
      continue;
    std::string semantic;
    for (auto *node = &d.model->data->nodes[d.node]; node; node = node->parent) {
      const std::string n = node->name ? node->name : "";
      for (const char *root : {"rs_unwinder_root", "rs_rewinder_root", "rs_dancer_module_root",
                               "rs_drive_module_root", "rs_hmi_root", "rs_main_cabinet_root"})
        if (n == root)
          semantic = root;
    }
    V3 lo{1e9f, 1e9f, 1e9f}, hi{-1e9f, -1e9f, -1e9f};
    for (int k = 0; k < 8; ++k) {
      auto v = point(d.model->world[d.node], {k & 1 ? d.max.x : d.min.x, k & 2 ? d.max.y : d.min.y,
                                              k & 4 ? d.max.z : d.min.z});
      lo = {std::min(lo.x, v.x), std::min(lo.y, v.y), std::min(lo.z, v.z)};
      hi = {std::max(hi.x, v.x), std::max(hi.y, v.y), std::max(hi.z, v.z)};
    }
    const float origin[3]{p->eye.x, p->eye.y, p->eye.z}, dir[3]{ray.x, ray.y, ray.z},
        l[3]{lo.x, lo.y, lo.z}, h[3]{hi.x, hi.y, hi.z};
    float t0 = 0, t1 = 1e9f;
    for (int a = 0; a < 3; ++a) {
      if (std::abs(dir[a]) < 1e-8f) {
        if (origin[a] < l[a] || origin[a] > h[a])
          t1 = -1;
      } else {
        float a0 = (l[a] - origin[a]) / dir[a], a1 = (h[a] - origin[a]) / dir[a];
        if (a0 > a1)
          std::swap(a0, a1);
        t0 = std::max(t0, a0);
        t1 = std::min(t1, a1);
      }
    }
    if (!semantic.empty() && t0 <= t1 && t0 < closest) {
      closest = t0;
      selected = semantic;
    }
  }
  return selected;
}
json Renderer::diagnostics() const {
  return {{"models", p->models.size()},
          {"draws", p->draws.size()},
          {"triangles", p->triangles},
          {"load_ms", p->load_ms},
          {"cpu_submit_ms", p->render_ms},
          {"morph_draws", p->morph_draws},
          {"live_path_vertices", p->live_path_vertices},
          {"shadow_map_size", p->shadows ? 2048 : 0},
          {"shadow_draws", p->shadow_draws},
          {"solid_support_draws", p->solid_support_draws},
          {"inward_joint_faces", p->joint_faces},
          {"support_winding_errors", p->support_winding_errors},
          {"rail_repaired_vertices", p->rail_repaired_vertices},
          {"support_rail_clearance_m", .002},
          {"instrument_updates", p->instrument_updates},
          {"instrument_force_n", p->instrument_force},
          {"instrument_source_time_s", p->instrument_time},
          {"live_hmi", p->live_hmi != 0},
          {"attention_draws", p->attention_draws},
          {"live_force_gauge", p->live_gauge != 0},
          {"near_plane_m", p->near_plane},
          {"far_plane_m", p->far_plane},
          {"camera", camera_pose()},
          {"gl_vendor", reinterpret_cast<const char *>(gl::GetString(GL_VENDOR))},
          {"gl_renderer", reinterpret_cast<const char *>(gl::GetString(GL_RENDERER))}};
}
void Renderer::webMarkers(bool enabled, float pitch_m) {
  p->web_markers = enabled;
  p->marker_pitch = std::clamp(pitch_m, .25f, 2.f);
}
std::vector<unsigned char> Renderer::scenePixels() const {
  std::vector<unsigned char> pixels(size_t(p->width) * p->height * 4);
  gl::BindFramebuffer(GL_FRAMEBUFFER, p->fbo);
  gl::ReadBuffer(GL_COLOR_ATTACHMENT0);
  gl::PixelStorei(GL_PACK_ALIGNMENT, 1);
  gl::ReadPixels(0, 0, p->width, p->height, GL_RGBA, GL_UNSIGNED_BYTE, pixels.data());
  gl::BindFramebuffer(GL_FRAMEBUFFER, 0);
  return pixels;
}
void Renderer::saveScene(const std::filesystem::path &path) const {
  auto pixels = scenePixels();
  stbi_flip_vertically_on_write(1);
  if (!stbi_write_png(path.string().c_str(), p->width, p->height, 4, pixels.data(), p->width * 4))
    throw std::runtime_error("Scene image write failed");
}
void Renderer::screenshot(const std::filesystem::path &path, int width, int height) {
  std::vector<unsigned char> pixels(size_t(width) * height * 4);
  gl::BindFramebuffer(GL_FRAMEBUFFER, 0);
  gl::ReadBuffer(GL_BACK);
  gl::PixelStorei(GL_PACK_ALIGNMENT, 1);
  gl::ReadPixels(0, 0, width, height, GL_RGBA, GL_UNSIGNED_BYTE, pixels.data());
  stbi_flip_vertically_on_write(1);
  if (!stbi_write_png(path.string().c_str(), width, height, 4, pixels.data(), width * 4))
    throw std::runtime_error("Screenshot write failed");
}
} // namespace slx::desktop
