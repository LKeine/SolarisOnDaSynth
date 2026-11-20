# SolarisOnDaSynth — plugin VST3 de mix pour synthés et mélos

Chaîne de mix pensée pour les synthés et les mélodies, sur **une piste seule** ou sur **un groupe** dans Ableton 12 (Windows).
L'objectif est un rendu à l'américaine (Mike Dean, Kanye, BNYX) : **nettoyer et embellir sans dénaturer le son d'origine**.

```
Entrée → Coupe-bas → EQ analogique (L/R ou M/S) → Saturation + Brillance → Compression → Stéréo → Delay → Reverb spatiale → Auto gain → Sortie
```

## ✦ Le bouton « ANALYSER LE SYNTHÉ »
Clique dessus et lance la lecture : le plugin écoute environ 6 secondes de son puis règle **tous les modules** tout seul.

| Ce qu'il mesure | Ce qu'il règle |
|---|---|
| Équilibre par octave (150 Hz → 20 kHz), comparé à un équilibre « mix US » | Coupe-bas, cut de boue (250–350 Hz) **ou** de dureté (3,5 kHz), air (8–11 kHz), corps |
| Attaques des notes : son percussif (pluck, piano, arp) ou tenu (pad) | Attaque/release du compresseur, type de satu, delay, mode de reverb |
| Dynamique (écart entre passages forts et faibles) | Seuil, ratio, drive |
| Part des basses | Clarté du compresseur, coupe-bas (un son plein de 808 n'est presque pas coupé) |
| Largeur stéréo | Élargit seulement un son trop étroit |
| Tempo du projet | Durée de la reverb et du predelay, notes du delay |

Les corrections valent la **moitié** de l'écart mesuré, et elles sont plafonnées : le son reste le tien.
Le bouton **ANALYSER** du compresseur ne règle que le compresseur.

## Modules
| Module | À quoi ça sert |
|---|---|
| **Coupe-bas** | 24 dB/oct, laisse de la place au 808 |
| **EQ analogique** | Les coupes restent propres ; quand tu **montes** une bande, elle ajoute des harmoniques (chaleur, soie). Dose avec **Couleur** |
| **Saturation** | *Chaleur* : Tape/Tube douce, harmoniques adoucies au-dessus de 11 kHz. *Brillance* : harmoniques générées seulement dans le haut du spectre |
| **Compression** | *Clarté* : les basses ne pilotent pas la compression. *Mix //* : compression parallèle, garde l'attaque des notes et ajoute de la puissance |
| **Stéréo** | Élargit le côté (side) au-dessus de *Basses mono* ; le centre n'est jamais modifié |
| **Delay** | Synchro au BPM, ping-pong, répétitions qui s'assombrissent |
| **Reverb spatiale** | Voir ci-dessous |

## Reverb spatiale (inspirée des reverbs vintage type Valhalla)
**Modes** : *Vintage Hall* (grande, sombre, chorus lent) · *Plate* (dense, brillante) · *Room* (petite, réflexions) · *Chamber* (chaleureuse) · *Cosmos* (immense, très modulée).

**Decay et predelay : à la main ou synchro au BPM**, avec la même logique que reverbcalculator.com :
- **Decay sync** : *Tight 1/4* · *Small Room 1/2* · *Big Room 1 mesure* · *Hall 2 mesures* · *Cosmos 4 mesures*. Le decay vaut la durée de la note moins le predelay.
- **Predelay sync** : *Auto* (durée totale ÷ 64, ou ÷ 128 en Tight, comme le calculateur), ou une valeur de note fixe (1/512, 1/128, 1/64, 1/32, 1/16).
- Sync désactivé → les boutons *Decay* (0,2–20 s) et *Predelay* (0–250 ms) reprennent la main.
- L'écran à droite affiche le decay et le predelay réellement appliqués, ainsi que le tempo.

Exemple à 120 BPM : Hall = 3 937,5 ms + 62,5 ms de predelay · Big Room = 1 968,75 + 31,25 · Small Room = 984,38 + 15,63 · Tight = 496,09 + 3,91.

*Low cut* (250 Hz par défaut) garde la reverb propre dans le grave ; *High cut* règle sa brillance ; *Modulation* donne le côté chorus et vintage.

## Barre d'outils
- **A / B** : deux jeux de réglages. Clique sur **B** pour essayer une autre version ; au premier passage, B part d'une copie de A. **A > B** copie le réglage affiché dans l'autre emplacement. A et B sont sauvegardés avec ton projet Ableton.
- **BYPASS (original)** : fait entendre le son d'origine, avec la même latence et un fondu de 20 ms, sans clic.
- **AUTO GAIN** (activé par défaut) : le plugin sort au **même volume perçu** que le son d'entrée (mesure BS.1770 sur environ 3 s, figée pendant les silences). Tu compares donc le son, pas le volume. Le gain appliqué s'affiche à côté (plafonné à ±12 dB). *Sortie* reste disponible pour ajuster.
- **DURÉE D'ANALYSE** : 6, 12 ou 24 s de son utile. Plus c'est long, plus l'analyse est fiable sur un morceau qui évolue.

## Vumètres
Le module **NIVEAUX** affiche l'entrée et la sortie en stéréo : crête, maintien de crête, et passage au rose au-dessus de −6 dB.

## EQ M/S
**Mode M/S** de l'EQ analogique : *Stereo* (gauche et droite), *Mid* (seulement le centre) ou *Side* (seulement les côtés).
Par exemple : de l'air en *Side* ouvre l'image sans durcir le lead au centre ; un cut de boue en *Mid* nettoie le centre sans toucher l'espace.

## Presets
- **Usine** : *Mélo simple* · *Groupe de mélos* · *Space Lead (Mike Dean)* · *Reset (neutre)*
- **Mes presets** : **Sauver** enregistre le réglage actuel, et **Suppr.** efface le preset utilisateur sélectionné.
  Les fichiers `.solaris` sont rangés dans `Documents\SolarisOnDaSynth\Presets`, et tu peux les copier vers un autre PC.
- Presets et A/B ne changent ni le bypass ni la durée d'analyse.

---

## Compiler (Windows)
**Sur ton PC** : Visual Studio 2022 Community (charge « Développement Desktop en C++ ») + Git, puis dans « Developer PowerShell for VS 2022 » :
```powershell
cmake -B build
cmake --build build --config Release
```
→ `build\SolarisOnDaSynth_artefacts\Release\VST3\SolarisOnDaSynth.vst3`

**Sans rien installer** : envoie le dossier sur GitHub. Le workflow `.github/workflows/build-windows.yml` compile le plugin, et tu télécharges l'artefact dans l'onglet *Actions*.

## Installer dans Ableton 12
1. Copie `SolarisOnDaSynth.vst3` dans `C:\Program Files\Common Files\VST3\`
2. Options → Paramètres → Plug-ins → « Utiliser le dossier système VST3 » → Réanalyser
3. Plug-ins → Solaris → SolarisOnDaSynth. Pour un groupe : sélectionne les pistes → **Ctrl+G** → plugin sur la piste de groupe.

## Outil de rendu hors-ligne (optionnel)
Le même moteur audio peut traiter un fichier WAV sans DAW :
```powershell
cmake -B build-tool -DSOLARIS_BUILD_PLUGIN=OFF -DSOLARIS_BUILD_RENDER_TOOL=ON
cmake --build build-tool --config Release
solaris_render entree.wav sortie.wav --bpm 88 --auto --match-level
```
Options : `--preset N`, `--set id=valeur` (ex. `--set eqMode=2`, `--set autoGain=0`), `--analyse-from secondes`, `--analyse-seconds N`, `--source groupe`.

## Organisation du code
- `Source/dsp/` : moteur audio indépendant de JUCE (paramètres, reverb, analyse, moteur)
- `Source/PluginProcessor.*` / `PluginEditor.*` : plugin JUCE et interface
- `tools/solaris_render.cpp` : rendu hors-ligne
