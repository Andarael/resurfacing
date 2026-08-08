

# Aplicación de demostración para resuperficialización procedural en tiempo real usando mesh shaders de GPU

Proyecto en GitHub: [Andarael/resurfacing](https://github.com/Andarael/resurfacing)

Enlace al artículo: [Resuperficialización procedural en tiempo real usando mesh shader de GPU\n](https://onlinelibrary.wiley.com/doi/abs/10.1111/cgf.70075)

Este proyecto demuestra el uso de mesh shaders de GPU para mejorar las superficies de objetos en tiempo real. La aplicación está escrita en C++ y GLSL, utilizando la API de Vulkan.

## Configuración probada

- **Sistema operativo**: Windows
- **GPU**: GPU NVIDIA (arquitectura RTX Ampere o Ada)
- **API gráfica**: Vulkan, con shaderc.
- **Sistema de compilación**: MS-Build y CMake
ADVERTENCIA: la configuración mínima requiere una GPU que admita Mesh Shader.
Intentar ejecutar esto en una máquina no Windows o en una GPU no NVIDIA es bajo su propio riesgo.

## Compilación

- Se proporciona un archivo `.bat` para generar una solución de Visual Studio.
- Los shaders se compilan automáticamente una sola vez durante la compilación.
- Nota: <!--Compilación de shaders y recarga en caliente --> La carga de mallas es mucho más rápida en la compilación de lanzamiento (release).

## Integración con ReShade

El paquete de Reshade puede usarse para efectos de postprocesado como la oclusión ambiental.
El código fuente de Reshade necesita ser modificado para hacerlo compatible con nuestro motor de Vulkan.
A la fecha del primer lanzamiento público de nuestra aplicación de demostración, aún no se proporciona integración con ReShade.
El soporte para ReShade está planeado para la próxima versión.

<!--
Para personalizar los efectos de postprocesado:
1. Coloque sus archivos de efecto `.fx` en la carpeta `reshade-shaders/shaders`. (p. ej., MXAO https://github.com/martymcmodding/iMMERSE/)
2. Inicie la aplicación y use la tecla `HOME` para abrir la interfaz de ReShade.
3. Habilite los efectos deseados y ajuste la configuración según sea necesario.-->

## Uso

<!--
### Carga de modelos de malla base

Los usuarios pueden cargar modelos en formato `.obj`. La aplicación incluye algunos modelos de ejemplo en la carpeta `models`.
Los modelos con armadura (skinned) son compatibles con el formato de archivo `GLTF` y se recopilan automáticamente al cargar desde un archivo `.obj` (esto se debe a una limitación de gltf que solo permite mallas triangulares).

Se pueden cargar varios modelos simultáneamente; haga clic en el botón `remove model_name.obj` para eliminar un modelo.

La malla base puede renderizarse por sí sola o junto con el modelo resuperficializado.
-->

### Superficies paramétricas simples

En la configuración de resuperficialización dentro de la configuración de modelos individuales, el usuario puede seleccionar el tipo de elemento que se utilizará para la resuperficialización.
Las propiedades paramétricas del elemento (como los radios del toro) pueden controlarse, así como la orientación y la escala.
La resolución máxima del elemento puede establecerse de forma independiente en las direcciones u y v.
De manera predeterminada, la función de mapeo F asigna un elemento por cara y un elemento por vértice.
Normal1 y Normal2 permiten que los elementos generados por vértice y por cara se orienten de diferentes maneras.

### Superficies basadas en jaulas de control

<!-- Las jaulas de control pueden cargarse desde un archivo `.obj`. -->
Cuando el tipo de elemento está establecido en `B-Spline' y se ha cargado una jaula de control, los elementos utilizarán la jaula de control para muestrear la superficie paramétrica.
Las jaulas de control deben proporcionarse en un formato de cuadrícula de cuadriláteros, similar al objeto *"Nurbs Surface"* de Blender.
La jaula de control predeterminada tiene forma de escala.

### Pebbles

La función de mapeo de pebbles asigna un único elemento por cara de la malla base.
Se puede controlar la redondez, el tamaño y la cantidad de extrusión.
Se puede habilitar el ruido procedural de superficie en los pebbles.

### LOD y Culling

- **LOD (Nivel de Detalle)**: Reduce dinámicamente la resolución del elemento en función del tamaño en el espacio de pantalla. Se pueden ajustar la resolución mínima y los factores de LOD.
- **Culling**: Elimina los elementos fuera del frustum de la cámara. El backface culling utiliza un cono normal, que puede ajustarse mediante el valor umbral.

### Métricas de rendimiento

Las métricas detalladas de rendimiento de la GPU se muestran como *"GPU Time"*, utilizando contadores precisos de GPU.
El rendimiento global de la aplicación (incluidas las operaciones de Skinning en CPU y sombreado) también se muestra en la barra superior.

## Licencia

Este proyecto está licenciado bajo la **Licencia Creative Commons Atribución-NoComercial 4.0 Internacional (CC BY-NC 4.0)**.

**Puede:**

- Compartir: Copiar y redistribuir el material en cualquier medio o formato.
- Adaptar: Remezclar, transformar y crear a partir del material.

**Bajo los siguientes términos:**

- **Atribución**: Debe otorgar el crédito correspondiente e indicar si se realizaron cambios.
- **NoComercial**: No puede utilizar el material con fines comerciales.

Detalles completos de la licencia: [CC BY-NC 4.0](https://creativecommons.org/licenses/by-nc/4.0/)
