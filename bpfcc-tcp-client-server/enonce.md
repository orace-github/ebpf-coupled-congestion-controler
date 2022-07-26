# Projet performance
## Un client et un serveur
### Prise en main du serveur et du client
Exécuter la version rapide et la version lente du client et du serveur. 
Tracer dans chaque cas les graphes des  fichiers de log de réception et des 
fichiers de log d’envoi. Interprèter et commenter les différences de performance.
Prendre les valeurs `delay=10`, `bw=100`, `loss_rate=0%` et `jitter=0%`

Maintenant nous allons nous concentrer sur la version rapide du serveur et du client.
### Variation du délai
Faire varier le délai d'un pas de 10 jusqu'à 100 et excécuter le serveur et le
client. Commenter les graphes obtenus à partir des fichiers de logs.

### Vartiation du taux de pertes
Faire varier le taux de perte d'un pas de `0.1%` à `2%`. Commenter les graphes.

### Variation du jitter
Faire varier le jitter d'un pas de `0.1%` à `2%`. Commenter les graphes.

## Plusieurs clients et un seul serveur
### Exécution manuelle
Le script `test.sh` permet de lancer plusieurs clients à la fois. Reprendre les
questions précédentes en exécutant le script `test.sh` pour 10, 20 , 50 
et 100 clients. Commenter les graphes obtenus.

## All in one
Automatiser tout cela dans le script python `gbeffasarl.py` de façon qu'à laide 
de la commande suivante il soit possible de générer les tests et les graphes
pour cahque cas.

```
 $ sudo python3 gbeffasarl.py <delay> <bw> <loss_rate> <jitter> 
```

# Buggs
Pour toutes questions ou malcompréhensions ou typos veuillez contacter 
assogba.emery@gmail.com ou faire un pull request.
