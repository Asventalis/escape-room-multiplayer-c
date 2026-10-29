# Compilazione dei file sorgente
make

read -p "Compilazione eseguita. Premi invio per eseguire..."


# Eseguo il client e due server (utilizzando la porta 4242)
TERM="gnome-terminal -x bash -c "

$TERM "./server 4242; exec bash" &
sleep 2 && ($TERM "./client 4242; exec bash" & $TERM "./client 4242; exec bash")

echo "Esecuzione avviata";
