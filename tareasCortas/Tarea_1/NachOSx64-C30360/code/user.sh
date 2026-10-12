# NachOS testing parameters
#
pwd > pwd.txt
hash=`awk -F/ -f ../user.awk pwd.txt`
rm -f pwd.txt
eval $hash > ../threads/.user.dat
cut -d: -f1 ../threads/.user.dat > ../threads/user.dat
curl https://os.ecci.ucr.ac.cr/check/carnet.php?carnet=`cat ../threads/user.dat` > ../threads/.uid.dat 2> /dev/null
