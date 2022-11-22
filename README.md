# TPE: 72.07 - Protocolos de Comunicación

# Proxy Server SOCKSv5 [RFC1928] 

## Autores
* **60072 - Malena Vásquez Currie**
* **60460 - Santiago Andrés Larroudé Alvarez**
* **60570 - Andrés Podgorny**
* **61278 - Sol Victoria Anselmo**


## Ubicación de los Materiales
Los archivos fuente .c se encuentran dentro de la carpeta
```
./src/
```
Mientras que los .h estan ubicados en
```
./include/
```
Los archivos ejecutables server y client se pueden encontrar en
```
./bin/
```

## Instrucciones de Compilación 
El proyecto se puede compilar por completo usando el comando make. 
```
make all
```
En caso de que se desee compilar únicamente el servidor o cliente:
```
make server
```
```
make client
```
Para realizar una limpieza completa
```
make clean
```

## Ejecución 
El server se puede correr usando
```
./bin/server -f <credentials_path> -u <user:pass>
```
```
./bin/server -f credentials.txt -u admin:grupo5
```
Consulte el manual para ver otros flags disponibles
```
man ./socksd.8
```
A su vez, si desea correr el cliente puede hacerlo con las credenciales que se encuentran en credentials.txt
```
./bin/client -u admin:grupo5
```
Consulte el manual para ver otros flags disponibles
```
man ./MASS.8
```




