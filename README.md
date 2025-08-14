# EXPOCENFO-2025
# Nombre proyecto: FANAX  

FANAX es un asistente personal inteligente de escritorio diseñado para adaptarse al entorno del usuario utilizando únicamente un microcontrolador **ESP32** y sensores básicos.  
Su objetivo es mejorar el enfoque y la productividad del usuario mediante la monitorización de variables ambientales simples, como la variación de luz o la posición de un potenciómetro (simulando interacción), y la generación de respuestas o sugerencias por medio de comunicación visual (LED) o conexión con modelos de lenguaje en la nube.  

A través de un sistema modular y evolutivo, FANAX aprende los hábitos del usuario y responde con retroalimentación contextual.

---

## Problemática a Resolver  
Muchos espacios de estudio y trabajo carecen de soluciones inteligentes de bajo costo que puedan adaptarse a los hábitos individuales sin requerir hardware complejo.  
La mayoría de asistentes personales actuales dependen de hardware avanzado o suscripciones costosas, dejando fuera a usuarios con recursos limitados.

---

## Objetivo General  
Desarrollar un asistente embebido minimalista que utilice un **ESP32** y sensores básicos para monitorear patrones simples del usuario y ofrecer recomendaciones personalizadas a través de una interfaz visual y conectividad con servicios de IA en la nube.

---

## Objetivos Específicos  
- Simular interacción del usuario mediante potenciómetro como entrada analógica.  
- Implementar patrones de monitoreo ambiental básico mediante el ADC del ESP32.  
- Integrar peticiones a una API de IA (como OpenAI o Gemini) para generar recomendaciones simples.  
- Representar estados del sistema y sugerencias mediante LED integrado o RGB externo.  
- Crear un sistema de “rutina adaptativa” que registre valores de actividad y genere retroalimentación con base en repeticiones o cambios.

---

## Funcionalidades Principales  
- **Lectura analógica continua** (por potenciómetro) para simular actividad/estado del usuario.  
- **Lógica de patrones**: detectar hábitos como duración de sesiones o interrupciones.  
- **Conexión WiFi y peticiones HTTP** a modelos de lenguaje para obtener sugerencias (por ejemplo: “Haz una pausa”, “Buen momento para retomar”).  
- **Señales visuales** usando LED para indicar estados de concentración, distracción o cambio de rutina.  
- **Posibilidad de expansión** hacia sensores reales en fases futuras.

---

## Innovaciones Técnicas  
- Implementación de **IA contextual** en sistemas embebidos con entrada/salida limitada.  
- Sistema de retroalimentación visual basado en lógica de patrones simples.  
- Uso del **ESP32** como puente entre la nube y el entorno físico del usuario sin necesidad de dispositivos adicionales.
