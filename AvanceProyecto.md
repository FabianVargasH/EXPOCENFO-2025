1. Información del proyecto
Nombre del proyecto: FANAX 
 
Equipo: Axel Chaves Olazo, Fabián Vargas Hidalgo, Andrés Meléndez Carvajal
 
Roles:
En este grupo, todos manejamos flexibilidad en cuanto a los roles, no los dividimos en tareas específicas sino que cuando se nos presenta una situación, decidimos que parte avanzar y nos ayudamos mutuamente.

2. Descripción y Justificación
Problema que se aborda:
 Muchos espacios de estudio y trabajo no tienen soluciones inteligentes de bajo costo que se adapten a hábitos individuales sin hardware complejo; los asistentes actuales suelen requerir equipos avanzados o suscripciones costosas, excluyendo a usuarios con recursos limitados.
Importancia y contexto:
 FANAX propone un asistente embebido minimalista (ESP32 + sensores básicos) que monitorea señales simples del entorno/usuario y devuelve sugerencias útiles por LED o mediante un LLM en la nube. Esto alinea el proyecto con la consigna de conectar sistemas que perciben el ambiente y que envían un prompt a un modelo de lenguaje para obtener una respuesta y accionar en el entorno.
Usuarios/beneficiarios:
 Estudiantes y trabajadores que buscan enfoque y productividad con soluciones accesibles y sin hardware costoso.

3. Objetivos del Proyecto
Objetivo General:
 Desarrollar un asistente embebido minimalista en ESP32 que, con sensores básicos, monitoree patrones simples y ofrezca recomendaciones personalizadas mediante LED y servicios de IA en la nube.
Objetivos Específicos:
Simular interacción de usuario con potenciómetro (entrada analógica).


Implementar monitoreo ambiental básico vía ADC del ESP32.


Integrar peticiones a una API de IA (OpenAI o Gemini) para recomendaciones.


Mostrar estados/sugerencias con LED integrado o RGB.


Crear una rutina adaptativa (registro de actividad y retroalimentación).



4. Requisitos Iniciales
Lista breve de lo que el sistema debe lograr:
Leer entrada analógica (potenciómetro) de forma continua para simular actividad del usuario.


Detectar patrones básicos (duración de sesiones, interrupciones) para dar contexto.


Conectarse por WiFi y hacer peticiones HTTP a un LLM (OpenAI/Gemini) y emitir una acción (mensaje/sugerencia y señal LED) en el entorno.










5. Diseño Preliminar del Sistema
Arquitectura inicial:

[ Sensores básicos ]            [ Bot/LED ]
      |                              ^
      v                              |
   (ADC ESP32)  -->  Lógica de patrones  -->  Estado/sugerencia (LED)
      |                   |
      |                   v
      |              Generación de prompt
      |                   |
      v                   v
  Conexión WiFi  --->  LLM/API en la nube  --->  Respuesta (texto/acción)

Flujo clave según instrucciones: microcontrolador percibe entorno → genera prompt → LLM responde → acción en el entorno.


Componentes previstos:
Microcontrolador: ESP32 (ADC, WiFi integrado).


Sensores/actuadores:


Entrada: potenciómetro (actividad simulada), opcional LDR o sensor simple de luz.


Salida: LED integrado o LED RGB externo para estados.


LLM/API: OpenAI o Gemini (vía HTTP).


Librerías y herramientas (referenciales): WiFi y cliente HTTP en ESP32 para solicitudes a la API; lógica de patrones en firmware. (Conectividad y HTTP están previstos en el proyecto).
(El proyecto está abierto a tomar cambios en los materiales, ya sean añadiduras o descartes de material).




6. Plan de Trabajo
Cronograma preliminar (fechas estimadas; hoy es 3 de agosto de 2025):

Hito
Entregable
Fecha estimada

H1
Lectura estable del potenciómetro (ADC) y mapeo de valores
08–12 ago 2025



H2
Lógica de patrones (detectar duración de sesión/interrupciones)
13–18 ago 2025



H3
Conexión WiFi + petición HTTP a LLM (endpoint de prueba)
19–23 ago 2025



H4
Integración respuesta→acción (mensajes + LED estados)
24–28 ago 2025



H5
Rutina adaptativa (registro simple y retroalimentación)
29 ago–04 sep 2025



H6
Pruebas, refinamiento y demo
05–08 sep 2025

Riesgos identificados y mitigaciones:
Riesgo 1: Latencia o fallas del LLM/API en la nube.
 Mitigación: cachear últimas sugerencias válidas y definir fallback local (mensajes básicos “toma un descanso/retoma ahora”). (Coherente con el diseño de conectar a la nube pero mantener salidas simples en LED).


Riesgo 2: Lecturas ruidosas del ADC / variabilidad en sensores.
 Mitigación: filtrado por ventana móvil o promedio exponencial, umbrales con histéresis para evitar cambios erráticos (sostiene la “lógica de patrones” prevista).


Riesgo 3: Configuración y seguridad de credenciales (API key).
 Mitigación: almacenar la API key fuera del firmware compilado (por ejemplo, por serie/OTA o en partición segura), limitar cuota y manejo de errores de autenticación. (Alineado con el flujo “microcontrolador → API por key”).


Riesgo 4: Conectividad WiFi inestable.
 Mitigación: reconexión automática, timeouts y modo degradado que mantenga la retroalimentación por LED sin LLM. 



