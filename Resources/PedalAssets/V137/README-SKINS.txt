ToneGrid 1.1.38 - PNG des pedales, revision polish-138

Ces PNG sont charges au lancement. Pour retoucher la couleur, la peinture ou
les illustrations : fermer ToneGrid, modifier le PNG en conservant exactement
son nom, ses dimensions, sa transparence et la position des controles/jacks,
puis relancer ToneGrid. Aucune recompilation n'est necessaire.

Windows/Linux : le dossier PedalSkins a cote de l'executable est directement
modifiable. Pour garder ses retouches lors des mises a jour, on peut copier
seulement les PNG modifies dans le dossier utilisateur :
Windows : %APPDATA%\ToneGrid\PedalSkins
macOS : ~/Library/Application Support/ToneGrid/PedalSkins
Linux : le sous-dossier ToneGrid/PedalSkins du repertoire de donnees utilisateur
choisi par JUCE (voir le dossier de donnees ToneGrid existant).
Sur Mac, utiliser le dossier utilisateur pour ne pas modifier l'app signee.

Priorite de lecture : PNG utilisateur, PNG fournis dans PedalSkins, puis image
integree dans l'executable. Un PNG absent, illisible ou de mauvaise taille est
ignore : l'image de secours est utilisee. Le controle ne change pas de place.
layout.json et provenance.json sont des informations techniques ; la geometrie
utilisee par cette version reste celle integree a l'executable.

face01.png : Classic Fuzz
face02.png : Treble Boost
face03.png : Auto Wah
face04.png : Studio Comp
face05.png : Transparent OD
face06.png : Horse OD
face07.png : 808 OD
face08.png : Blues OD
face09.png : Filter Dist
face10.png : Orange Dist
face11.png : Vintage Fuzz
face12.png : Graphic EQ
face13.png : Clean Boost
switch_high.png et switch_low.png : footswitch commun.

Dimensions : 1024 x 1536 pour toutes les faces, sauf face12.png (1324 x 1536).
Ne pas ajouter de traits aux knobs, LED, footswitches, listes deroulantes ou
poignees aux faders : ces elements sont dessines par ToneGrid.
Les corps des knobs, les textes et les jacks font partie du skin.

La reference originale fournit les couleurs, le grain et les illustrations.
Les contours mats et angles peu arrondis suivent la Classic Fuzz validee.
Le point de contact des jacks est commun et permet leur raccord a toute taille.
