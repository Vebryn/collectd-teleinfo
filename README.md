# collectd-teleinfo
collectd-teleinfo est un programme qui lit les trames teleinformation (tic) d'un port série.

Les sources proviennent de domos78. Les sources présentent quelques différences :
* plus d'export en csv
* mode debug désactivé
* passage en monophasé
* affichage des valeurs IINST, PAPP, HCHC et HCHP sur la sortie standard au format collectd

Le site de vesta détaille le programme initial ainsi que sa version qui enregistre les données dans une base mysql.
http://vesta.homelinux.free.fr/wiki/demodulateur_teleinformation_edf.html

# Configuration collectd

L'interface avec collectd se réalise avec le plugin exec. Le programme est exécuté par collectd à intervalles réguliers. Il lit et affiche les données teleinfo sur la sortie standard au format collectd :

    PUTVAL "pi/exec-teleinfo/gauge-hchc" interval=60 N:1951966
    PUTVAL "pi/exec-teleinfo/gauge-hchp" interval=60 N:1110786
    PUTVAL "pi/exec-teleinfo/gauge-iinst" interval=60 N:11
    PUTVAL "pi/exec-teleinfo/gauge-papp" interval=60 N:2500

Les scripts interprétés qui s'interfacent avec collectd doivent rester en mémoire pour éviter de charger l'interprêteur à chaque intervalle (> 2,5 Mo). L'empreinte mémoire de collectd-teleinfo se limite à 0,7 Ko. Il peut se permettre d'être chargé à chaque intervalle.

    LoadPlugin exec
    <Plugin exec>
      Exec "teleinfo:dialout" "/usr/local/bin/teleinfo"
    </Plugin>

Idéalement, collectd-teleinfo devrait être un plugin natif collectd.
