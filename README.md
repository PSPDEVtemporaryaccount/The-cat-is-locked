# The Cat Is Locked — PSP Beta 0.5.2

## Refonte des menus

- Le menu principal utilise désormais `assets/Pictures/Menu_principal_asset.png`.
- **Jouer** ouvre le menu JOUER.
- Dans le menu JOUER, **Reprise de la partie** ouvre les cinq emplacements de sauvegarde.
- Dans le menu JOUER, **Commencer l'aventure** démarre directement le gameplay, sans menu intermédiaire.
- **Paramètres** ouvre le menu correspondant : sons, volume et réinitialisation.
- La réinitialisation ouvre une confirmation : **Non** revient aux paramètres sans rien effacer ; **Oui** efface les cinq sauvegardes.
- Les sauvegardes sont stockées dans `ms0:/PSP/SAVEDATA/TCLCK0001/DATA.BIN`. Une nouvelle aventure utilise le prochain emplacement libre ; cinq sauvegardes maximum sont autorisées.
- Les images PNG sont décodées et converties en textures RGBA8888 par `tools/prepare_assets.py` pendant la compilation. Le script n'a besoin que de Python standard.

## Commandes

- Croix directionnelle et stick analogique gauche : déplacement du chat.
- Croix : valider une option.
- Haut/Bas : naviguer.
- Gauche/Droite : modifier les sons ou le volume dans les paramètres.
- Rond pendant le jeu : sauvegarder puis revenir au menu principal.
- Rond dans les menus : retour.

## État du prototype

Le gameplay reste un prototype : pièce 3D simple, chat en géométrie primitive, deux clés et quatre pièges. Les sauvegardes conservent la position, la vie, les clés et les pièges déclenchés. La progression de niveau et les effets sonores ne sont pas encore implémentés.

La compilation GitHub Actions valide la construction de l'EBOOT ; une vérification visuelle dans PPSSPP/JSPSP reste nécessaire pour confirmer le rendu sur émulateur.


## Beta 0.5.2 — gameplay prototype update

- Difficulty selection is shown before starting a new game: Facile (5 vies), Normal (3), Difficile (2), Extrême (1), Hardcore (1 essai, aucune vie supplémentaire) and Entraînement (vies illimitées).
- The health bar remains separate from the life count. Traps reduce health; when health reaches zero, the player loses one attempt and returns to the beginning of the current level. Training mode restores the player without consuming a life.
- The first prototype room uses one key and a locked exit. Reaching the exit after collecting the key completes the level and saves a checkpoint at the next level's start.
- The prototype advances through 10 numbered levels per world and 6 world counters. At this stage the room geometry is still a placeholder reused for each level; worlds 2–6 do not yet have their final environments or unique mechanics.
- Hardcore has one total attempt. Losing it clears the active run and returns the next attempt to World 1, Level 1. Other difficulties keep their remaining attempts in the save slot.
- The main menu artwork and layout are unchanged. Only its arrow indicators now use the two original yellow arrow designs from `assets/Assets/`.
- Save format version 2 stores world, level, difficulty, lives, health, keys and trap state. Existing version 1 save files are not migrated and will be treated as empty.
