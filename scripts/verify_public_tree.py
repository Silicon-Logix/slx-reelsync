# SPDX-FileCopyrightText: 2026 Marco Pezzullo (Silicon LogiX)
# SPDX-License-Identifier: LicenseRef-SLX-ReelSync-Demo-Evaluation-1.0
"""Audit the standalone distribution before committing or publishing it."""
from pathlib import Path
import argparse, hashlib, json, re, struct

RUNTIME = {
 'scene/SLX_ReelSync_Runtime.glb',
 'branding/siliconlogix-logo-light-transparent.png', 'branding/reelsync.png',
 'config/camera_presets.json', 'config/kinematic_samples.json', 'config/scene_contract.json'}
FORBIDDEN = {'private','workspace','build','design','embedded','SLX_ReelSync','__pycache__','node_modules'}
EXTENSIONS = {'.blend','.blend1','.blend2','.gltf','.fbx','.usd','.usda','.usdc','.obj','.o','.pdb','.ilk','.dump','.key','.pem','.log'}

def audit(root, publish=False):
 root=Path(root).resolve();issues=[];count=0;vendor=0
 licensing=json.loads((root/'FILE-LICENSES.json').read_text(encoding='utf-8'))
 entries=licensing.get('files',{})
 files=[p for p in root.rglob('*') if p.is_file() and '.git' not in p.relative_to(root).parts]
 for p in files:
  rel=p.relative_to(root).as_posix();parts=p.relative_to(root).parts;count+=1
  if p.is_symlink():issues.append(rel+': symbolic link')
  if any(x in FORBIDDEN for x in parts) or p.suffix.lower() in EXTENSIONS:issues.append(rel+': private/build/authoring file')
  if p.suffix.lower()=='.glb' and rel!='assets/scene/SLX_ReelSync_Runtime.glb':issues.append(rel+': additional 3D asset')
  if rel.startswith('assets/') and not rel.endswith('.license') and rel[7:] not in RUNTIME:issues.append(rel+': non-runtime asset')
  if rel.startswith('third_party/') and p.suffix.lower() in {'.exe','.dll','.lib','.a','.ttf','.otf','.woff','.woff2','.png','.jpg','.jpeg','.zip'}:issues.append(rel+': unused vendor binary/font/asset')
  if rel not in entries:issues.append(rel+': missing license attribution')
  data=p.read_bytes()
  for expression in [rb'[cC]:[\\/]Users[\\/]',rb'/home/'+b'mpezzullo/',rb'-----BEGIN (?:RSA |EC |OPENSSH )?PRIVATE KEY-----',rb'ghp_[A-Za-z0-9]{36,}',rb'github_pat_[A-Za-z0-9_]{40,}']:
   if re.search(expression,data):issues.append(rel+': local path or credential marker');break
  if rel.startswith('third_party/') or rel.startswith('licenses/'):vendor+=1;continue
  if not rel.endswith('.license') and 'SPDX-License-Identifier:' not in data[:2000].decode('utf-8',errors='ignore'):
   if not p.with_name(p.name+'.license').is_file():issues.append(rel+': missing SPDX header/sidecar')
 glb=root/'assets/scene/SLX_ReelSync_Runtime.glb';data=glb.read_bytes()
 doc=json.loads(data[20:20+struct.unpack_from('<I',data,12)[0]])
 if any(x.get('uri') and not x['uri'].startswith('data:') for k in ('buffers','images') for x in doc.get(k,[])):issues.append('GLB: external URI')
 blockers=[]
 for name in ('LICENSE','SDK_LICENSE','ASSET_LICENSE'):
  if 'DRAFT' in (root/name).read_text(encoding='utf-8'):blockers.append(name+' is still an owner-review draft')
 status=json.loads((root/'PUBLISHING_STATUS.json').read_text(encoding='utf-8'))
 if status.get('asset_redistribution_confirmed') is not True:blockers.append('Runtime asset redistribution is not confirmed')
 if status.get('license_terms_finalized') is not True and not any('owner-review draft' in x for x in blockers):blockers.append('SLX license terms are not confirmed as finalized')
 if publish and blockers:issues.extend(blockers)
 report={'status':'PASS' if not issues else 'FAIL','files_checked':count,'upstream_files_preserved':vendor,'runtime_asset_count':len(RUNTIME),'private_or_authoring_files_found':bool(issues),'publication_ready':not blockers,'publication_blockers':blockers,'issues':issues,'runtime_glb_sha256':hashlib.sha256(data).hexdigest()}
 return report

def main():
 p=argparse.ArgumentParser();p.add_argument('root',nargs='?',type=Path,default=Path(__file__).resolve().parents[1]);p.add_argument('--require-publishable',action='store_true');a=p.parse_args()
 r=audit(a.root,a.require_publishable);print(json.dumps(r,indent=2));raise SystemExit(0 if r['status']=='PASS' else 1)
if __name__=='__main__':main()
