# Hopefully this script works... If it doesn't, sorry, but I just started with C!

echo "Compiling \`$1\` to \`$2\`..."
gcc -Wall -O2 -D_DEFAULT_SOURCE -D_XOPEN_SOURCE=600 $1 -Wl,-Bsymbolic-functions -lncurses -ltinfo -lm -pthread -o $2

if [ $? -ne 0 ]; then
    echo -e "\nEncountered an error, did not compile!"
else
    echo -e "\nDone!"
fi