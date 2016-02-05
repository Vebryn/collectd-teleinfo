# collectd-teleinfo
print teleinfo to stdout (collectd format)

Fork du script de domos78, différences :
* plus d'export en csv
* mode debug désactivé
* passage en monophasé
* affichage des valeurs IINST, PAPP, HCHC et HCHP sur la sortie standard au format collectd

Source : http://vesta.homelinux.free.fr/wiki/demodulateur_teleinformation_edf.html#Programmes

# Prochaines versions

Conversion en vrai plugin collectd

# Configuration collectd

LoadPlugin exec
<Plugin exec>
  Exec "teleinfo:dialout" "/usr/local/bin/teleinfo"
</Plugin>
