# The Cat Is Locked — PSP Beta 0.8.0

## Travail de refonte — état de cette build

La Beta 0.8.0 commence à retirer les menus constitués d'images. Les PNG de référence restent dans `assets/Pictures`, mais ils ne sont plus embarqués dans l'EBOOT ni affichés comme écrans de menu.

### Interface native
- Écrans et panneaux de menu dessinés en primitives GU et texte PSP.
- Sélection de menu avec Croix et navigation Haut/Bas.
- Paramètres : son, volume musique, volume ambiance et remise à zéro (les commandes de réglage sont traitées dans la boucle menu).
- Les PNG dans `assets/Pictures` restent des références artistiques, pas des textures de menus dans cette build.

### Chat et gameplay
- Le chat est maintenant composé de maillages ellipsoïdes polygonaux arrondis plutôt que d'un assemblage de cubes.
- Le niveau reste un prototype géométrique 3D : murs, porte, obstacles et mobiliers utilisent encore des primitives simples. Un modèle félin détaillé, des animations complètes et une finition de niveau commerciale ne sont pas encore présents.
- Déplacement au stick analogique ou à la croix directionnelle.
- Clé, porte, pièges, progression, sauvegardes et pause restent dans la base existante.

### Audio
- La musique des menus est décodée par FFmpeg en PCM mono à 44 100 Hz.
- Le flux audio PSP et la durée du fondu utilisent 44 100 échantillons/seconde, pour éviter le problème de lecture accélérée lié à un taux d'échantillonnage incorrect.
- La vitesse audio doit encore être confirmée par un essai d'exécution PPSSPP ou sur PSP réelle.

### Compilation et validation
GitHub Actions prépare uniquement le PCM audio, compile avec PSPSDK et publie l'artefact `The-Cat-Is-Locked-Beta-0.8.0`.

La réussite de la compilation ne vaut pas validation du rendu ni du gameplay. Aucun test runtime PPSSPP/PSP n'est revendiqué tant qu'un émulateur n'a pas réellement lancé l'EBOOT.
