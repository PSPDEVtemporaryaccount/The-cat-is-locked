# The Cat Is Locked — PSP Beta 0.7.2

## Refonte complète

La Beta 0.7.2 repart d'une base de gameplay neuve. L'ancien prototype provisoire et ses anciens indicateurs de menu ne sont plus utilisés.

### Menu
- Les visuels récents V2 sont intégrés aux écrans principaux, Jouer, Reprendre et Paramètres.
- Le menu principal utilise les quatre visuels Menu_principal_V2.
- La sélection visuelle est intégrée dans les images : aucune flèche ajoutée par le code.
- Les sous-menus utilisent les images du dossier assets/Pictures/Game Menus.
- Croix : valider ; Haut/Bas : naviguer ; Rond : retour.
- Le menu Paramètres permet de modifier le son et le volume (le système audio n'est pas encore branché).

### Gameplay jouable
- Déplacement du chat au stick analogique ou à la croix directionnelle.
- Une pièce 3D avec murs, porte, mobilier/obstacles et chat composé de volumes 3D.
- Une clé à trouver dans chaque niveau.
- La porte reste fermée avant la récupération de la clé ; rejoindre la porte après l'avoir récupérée termine le niveau.
- Les obstacles changent selon le niveau et bloquent le déplacement.
- 6 mondes de 10 niveaux, avec sauvegarde automatique de la progression.
- Difficultés : facile (5 vies), normal (3), difficile (2), extrême (1), hardcore (1 essai) et entraînement (vies illimitées).
- Pause avec reprise, recommencer le niveau ou retour au menu.
- Cinq emplacements de sauvegarde sur Memory Stick. Les sauvegardes Beta 0.7.0 sont migrées vers le nouveau format.

### Commandes
- Stick analogique ou croix directionnelle : déplacer le chat.
- Croix : valider dans les menus.
- Haut/Bas : naviguer.
- Rond : retour dans les menus.
- START : pause pendant le jeu.

### Compilation et validation
GitHub Actions prépare les textures RGBA8888 à partir des PNG du dépôt, compile avec PSPSDK et publie l'artefact The-Cat-Is-Locked-Beta-0.7.0.

La compilation automatique confirme que l'EBOOT est construit. Le rendu et les sensations de jeu doivent encore être vérifiés sur PPSSPP ou une PSP réelle ; aucune validation d'exécution n'est revendiquée avant ce test.
