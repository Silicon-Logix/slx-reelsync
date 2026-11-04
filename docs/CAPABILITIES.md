<!-- SPDX-FileCopyrightText: 2026 Marco Pezzullo (Silicon LogiX)
SPDX-License-Identifier: LicenseRef-SLX-ReelSync-Demo-Evaluation-1.0 -->
# Funzioni e limiti della versione 1.4.0

Copyright (c) 2026 Marco Pezzullo (Silicon LogiX).

| Funzione | Stato |
| --- | --- |
| PC Windows x64, offline | Implementata e provata localmente |
| Core C++ DLL / SDK C ABI 1 | Implementato, capacità applicate dal core |
| Due zone elastiche, bobine variabili, nip, ballerino | Modello ridotto implementato |
| Conservazione / convergenza / seed | Verifiche numeriche interne |
| 3D live, morph bobine, percorso dinamico | Implementato, geometria generica |
| Presentazione cinematica di 60 s | Separata dal modello, senza grafici inventati |
| Ricette, riferimenti, stati, pausa e singolo passo | Implementati |
| Didattico C e confronto esportato | Implementati, modulo compilato con l'host |
| Guasti encoder, bias, attrito, rottura | Implementati secondo MODEL.md |
| Derating, forza pneumatica, scala encoder, cella bloccata, ritardo aggiuntivo | Implementati nel core |
| Inizio/durata, rimozione causa, memoria e conferma allarmi | Implementati, piano nell'host con conferme core |
| Campagna 12 scenari e confronto in background | Implementati, sessioni indipendenti |
| Errori operatore: riferimento fuori campo e manutenzione in marcia | Comandi inviati e validati dal core |
| About dedicato, icona e launcher EXE | Implementati, versioni e risorse verificate |
| About con banner identita e pulsanti a contrasto | Implementati, stile coerente con gli altri progetti SiliconLogiX |
| Riferimenti mobili sulla lastra | Implementati, coordinate materiale del core e pausa coerente |
| Arresto a metratura | Implementato nell'host; anticipo nominale e scarto reale nel report |
| Trigger pausa analisi al primo allarme | Implementato, singolo scatto e riarmo esplicito |
| Catalogo Su richiesta e fonti | Implementato; cinque proposte da sviluppare, scheda copiabile |
| Orbita, pan, focus, dieci preset e sei viste utente | Implementati |
| Materiali a contrasto, luci, ombre ed esposizione | Implementati; aspetto visuale, non fotometria qualificata |
| CSV, JSON, report, riproduzione CLI | Implementati, con snapshot telemetria ed eventi |
| Salvataggio della prova in corso e recupero alla riapertura | Implementati; replay verificato, ripartenza in pausa |
| Archivio automatico al cambio prova | Implementato; ricette pre-avvio da salvare esplicitamente |
| Layout e DPI | Verifiche locali: 1050×680, 1280×720, 1920×1061, 2560×1061; scala 100%, 125%, 150% |
| Rumore/ritardo/bias tensione, ritardo encoder | Implementati |
| Rumore diametri/posizione e slip encoder | Non implementati |
| Catenaria dello slack e caduta dei frammenti | Non implementate; percorso guida nominale |
| HMI e quadrante forza 3D live | Implementati; processo, qualità sensori e forza firmata dell'attuatore in N |
| Contratti snapshot/eventi/comandi | Implementati e verificati; decoder e trasporto TLS/WebSocket opzionale |
| Firma binari / entitlement firmati | Non disponibili in questa build |
| Linux desktop | Struttura portabile, build non provata |
| Raspberry / Wi-Fi | App C++/LVGL, servizio TLS, immagine Buildroot e prove ARM eseguite; collaudo fisico della nuova immagine pendente |
| Associazione e controllo remoto | Identità verificate, conferma sul PC, singola autorità, scadenze e nessun reinvio automatico |
| Calibrazione reale, modelli avanzati, autotuning, PLC, SIL/HIL | Da sviluppare su progetto |

Profili offline: bit 1 simulazione, 2 didattico, 4 guasti, 8 percorso live.
Nessuna safety, controllo hard real-time o prestazione produttiva certificata.
