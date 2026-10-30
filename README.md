# The Cat Is Locked — PSP Beta 0.1.1

Deuxième étape du prototype PSP / PPSSPP, avec un menu principal graphique.

## Commandes
- Flèches directionnelles : naviguer dans les menus et déplacer le personnage.
- Croix : valider.
- Rond : revenir en arrière ou quitter le niveau vers l'accueil.
- Dans Paramètres : gauche/droite pour activer ou désactiver les sons.

## Menus
- Menu principal : Jouer, Paramètres, Quitter.
- Jouer : Reprise, Commencer l'aventure.
- Paramètres : Vibreur (indisponible sur cette version), Sons, réinitialiser les sauvegardes.

## Changements de la Beta 0.1.1
- Ajout d'un panneau de menu graphique avec boutons et surbrillance de la sélection.
- Séparation visuelle de l'accueil, des sous-menus et du niveau.
- Suppression du réglage de volume qui n'était pas réellement relié à un système audio.
- Nettoyage de la navigation des menus.

## Limites connues
Le personnage visible dans cette version reste un modèle 3D provisoire et anguleux : ce n'est pas encore le chat réaliste souhaité. La sauvegarde est temporaire et disparaît à la fermeture du jeu. Les sons ne sont pas encore intégrés ; le réglage Sons prépare seulement l'option.

Les images de référence sont dans `assets/Pictures/`, notamment `Cam.jpeg`.

La compilation est lancée automatiquement par GitHub Actions à chaque push sur `main`.
