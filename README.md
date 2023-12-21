# follow_line_setr

Esta práctica se debe ejecutrar en el *ELEGOO Smart Robot Car V4.0 with Camera*, haciendo uso de una placa **Arduino UNO**, una **ESP32** y sensores de ultrasonidos, infrarrojo, motores y un LED RGB.

Consta de dos programas para sendas placas: mientras que el Arduino se encarga del movimiento del robot y procesamiento de los datos recogidos por los sensores, la ESP se encarga de la conexión WiFi y comunicación MQTT con un servidor.

## ESP32
El primer paso para que ambos códigos puedan empezar a ejecutarse es conectarse al WiFi y al servidor con MQTT para poder mandar y recibir mensajes. Para ello, se incluyen las librerías *<WiFi.h>*, *<Adafruit_MQTT.h>* y *<Adafruit_MQTT_Client.h>*.

Es necesario crear objetos que utilicen estas librería, para poder conectarse y publicar:
```c++
WiFiClient client;

Adafruit_MQTT_Client mqtt(&client, mqttServer, mqttPort);
Adafruit_MQTT_Publish channel = Adafruit_MQTT_Publish(&mqtt, topic);
```

**NOTA:** El código para conectarse al WiFi ha sido proporcionado por el profesor, ya que la red a la que había que conectarse era 'eduroam', la cual precisa de un usuario y contraseña para poder acceder.

```c++
const char* ssid = "eduroam";

setup() {
  // [...]
  
  // -------- WiFi connection --------
    WiFi.disconnect(true); 
  
    WiFi.begin(ssid, WPA2_AUTH_PEAP, EAP_IDENTITY, EAP_USERNAME, EAP_PASSWORD); 
  
    while (WiFi.status() != WL_CONNECTED) {
      delay(500);
    }
  
  // -------- MQTT connection --------
    mqtt.connect();
    if (mqtt.connected()){
      // Notify Arduino of succesfull WiFi and MQTT connection
      Serial2.println(send); 
    }
}

```
Las constantes *'EAP_...'* se definen en un fichero .h guardado en el mismo directorio que el programa, e incluido con el resto de librerías:
```c++
#include "user_credentials.h" // Personal credentials for 'eduroam' connection
```
Esto se implementa como me medida de seguridad, para proteger los datos personales y sensibles.

```c++ user_credentials.h
#ifndef USER_CREDENTIALS_H
#define USER_CREDENTIALS_H

// Variable definition
const char* EAP_IDENTITY = "USER@alumnos.urjc.es";
const char* EAP_USERNAME = "USER@alumnos.urjc.es";
const char* EAP_PASSWORD = "PASSWORD";

#endif
```

donde 'USER' y 'PASSWORD' se corresponden con el usuario y contraseña con el que se accede a los servicios de la universidad.

Para asegurarnos de que la conexión con MQTT no se perdía o, en caso de hacerlo, que fuese capaz de reconectarse, en el *'loop'* del programa se llama a la función **reconnectMQTT**, la cual implementa una sencilla (pero importantísima) condición:
```c++
if(!mqtt.connected()){
  mqtt.connect();
}
```
Una vez se tiene implementada esta conexión, es necesaria la **comunicación serie**. Esta es la que permite a ambas placas mandar y recibir mensajes entre ellas.

Para ello:
- se definen los puertos
```c++
// Serial communication ports
#define RXD2 33
#define TXD2 4
```
- se inicializa la comunicación en el *setup*
```c++
Serial2.begin(9600, SERIAL_8N1, RXD2, TXD2);
```

Ahora, se pueden enviar mensajes al Arduino con *'Serial2.println'* y recibir con *'Serial2.read'*.

El primer mensaje que se intercambiará será para notificar al Arduino que la comunicación se ha establecido correctamente. Para ello, en el *setup* se envía un *char send = 'c'* (connected).

A partir de este momento, la implementación es bastante sencilla: se publicarán por MQTT aquellos mensajes que el Arduino indique, además de publicar un **ping** cada *(aprox.)* 4 segundos.

Para diferenciar que mensaje enviar, el Arduino mandará un caracter. En el caso de recibir:
- **'s'**  (start) :

Además de publicar el primer mensaje, se inicializan variables encargadas de guardan timestamps. Estas se utilizan para calcular el tiempo total en el que el programa está en ejecución y los lapsos entre cada ping.
```c++
channel.publish(init_circuit);

// Initialize variables
init_time = millis();
current_time = millis();
ping_time = 0;
```

- **'l'**  (lost) :
```c++
channel.publish(lost_circuit);
channel.publish(init_search);
```

- **'f'**  (found) :
```c++
channel.publish(line_found);
channel.publish(end_search);
```

- **'0' a '9'**  (found) :

El caracter que se recibe representa la distancia a la que se ha detectado el obstáculo. Esta distancia se concatena al mensaje enviado:
```c++
// Concatenate number (distance) to string and publish
snprintf(obstacle, sizeof(obstacle), "%s00%c\n}\n", detect_dist, received);
channel.publish(obstacle);
```
Una vez enviado este mensaje, se calcula el tiempo total que ha estado en funcionamiento el robot sigue-líneas, al igual que con la distancia, se concatena al mensaje a enviar:
```c++
// Calculate total time
int end_time = millis();
int total_time = end_time - init_time;

char final[100];
// Concatenate number (time) to string and publish
snprintf(final, sizeof(final), "%s%d\n}\n", end_circuit, total_time);
channel.publish(final);

// End program
while(1);
```
**NOTA:** debido a las caracteristicas de la placa, la única forma de parar la ejecución del programa es crear un bucle infinito dentro del *loop* (véase bucle while(1))

Todas las publicaciones de mensajes anteriores se hacen dentro del mismo *if*, que comprueba si se reciben o no datos del Arduino.
Sin embargo, el mensaje del **ping**, al ser independiente de la otra placa, se implementa fuera.
```c++
// Check if there is data available on Serial2
if (Serial2.available() > 0) {
  received = Serial2.read();

  // [...]

}
// Check value of ping_time even if no messages are been received
if (ping_time <= 4005 && ping_time >= 3995) {
  // Update value
  current_time = millis();

  // Calculate total time
  int total_ping_time = current_time - init_time;
  
  char ping[100];
  // Concatenate number (time) to string and publish
  snprintf(ping, sizeof(ping), "%s%d\n}\n", ping_msg, total_ping_time);
  channel.publish(ping);
}
```

Para calcular el valor de la variable *ping_time*, al principio del *'loop'* se ejecuta el siguiente código:
```c++
// Calculate ping_time (must be aprox. 4s)
check_time = millis();
ping_time = (check_time - current_time);
```

## ARDUINO UNO

Bucle Principal y Seguimiento de Líneas

El bucle principal del programa Arduino es donde se gestiona el seguimiento de línea y la comunicación con la ESP32. Dentro de loop(), el programa realiza constantemente las siguientes acciones:

Lectura de Sensores: Se leen los sensores infrarrojos para detectar la línea y ajustar el movimiento del robot consecuentemente.  
Control de Motores: En función de la información de los sensores, se ajusta la velocidad y dirección de los motores para seguir la línea.  
Detección de Obstáculos: Se utiliza el sensor de ultrasonidos para identificar obstáculos en el camino y detenerse.  
Comunicación Serie: Se envían y reciben datos a través de la comunicación serie con la ESP32 para coordinar acciones y compartir información.

### Seguimiento de Líneas
El robot utiliza sensores infrarrojos para detectar y seguir una línea. Esta funcionalidad se basa en la lectura de los valores de los sensores colocados estratégicamente en el robot. Dependiendo de cuál sensor detecta la línea, el programa ajusta el movimiento de los motores para mantener al robot alineado con la línea. Para el movimiento cuando solo uno de los dos sensores externos detecta la linea (hay que corrgeir el ángulo) hemos utilizado un PD, donde nuestro error es la diferencia entre el valor analógico de los sensores infrarojos

### Caracteres Enviados
Durante la ejecución, el Arduino envía caracteres específicos a la ESP32 para indicar diferentes estados o eventos:

's' (start): Indica que el robot ha comenzado a seguir la línea.
'l' (lost): Se envía si el robot pierde la línea, indicando la necesidad de buscarla nuevamente.
'f' (found): Se transmite cuando el robot vuelve a encontrar la línea después de haberla perdido.
'0' a '9': Estos caracteres representan la distancia detectada a un obstáculo, siendo enviados cuando se encuentra uno.


Ejemplo de Implementación en loop()
Aquí se muestra un fragmento simplificado del código dentro del loop() que ilustra el seguimiento de líneas y la comunicación con la ESP32:

```c++
void loop() {
  // Lectura de sensores infrarrojos
  bool leftSensor = digitalRead(PIN_ITR20001_LEFT);
  bool middleSensor = digitalRead(PIN_ITR20001_MIDDLE);
  bool rightSensor = digitalRead(PIN_ITR20001_RIGHT);

  // Lógica de seguimiento de líneas
  if (ningunSensor) {
    // Recuperar linea girando rápidamente en el último sentido de giro
  } else if (onlyMiddleSensor) {
    // Mantener dirección
  } else {
    // Girar en base al error
  }

}

```

