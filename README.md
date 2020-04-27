# Visualizador de archivos Wavefront .obj

### Descripción General
<p>El programa carga cualquier archivo Wavefront .obj cuya maya esté triangulada y tenga normales, le aplica una textura tipo checker, renderiza el modelo y lo despliega en una ventana del gestor X con un par de luces predeterminadas. Además, permite rotar el modelo sobre los ejes X y Y (en coordenadas de plano de proyección), así como desplazar la cámara por el mundo.</p>
<p>No se utilizó ninguna biblioteca de gráficos. Sólo utiliza las bibliotecas Eigen y X11.</p>
<p>A continuación, se muestra un ejemplo de render generado con el programa.</p>
<img alt="Render del conejo de Stanford." height="300px" width="300px" src="https://raw.githubusercontent.com/Davacas/Visualizador-Archivos-OBJ/master/renders/render_conejo.png">
<p>En la carpeta "renders" de este repositorio se encuentran algunos de los renders de diferentes archivos, generados con este programa.</p>

### Código fuente y bibliotecas externas
<p>El programa se escribió en C++ sobre Ubuntu 18.04 y sólo requiere de las bibliotecas de desarrollo de X11 y de la biblioteca para operaciones de álgebra lineal Eigen. No utiliza ninguna biblioteca de gráficos.</p>
<p>En Ubuntu (y probablemente en otras distribuciones Debian) se pueden descargar e instalar las bibliotecas de desarrollo de X11 mediante el comando de consola:</p> 
<p><code>sudo apt install libx11-dev</code></p>
<p>La última versión estable de Eigen se puede descargar de: <a href="https://gitlab.com/libeigen/eigen/-/archive/3.3.7/eigen-3.3.7.zip">https://gitlab.com/libeigen/eigen/-/archive/3.3.7/eigen-3.3.7.zip</a>. Una vez descargada, se puede descomprimir y colocar en cualquier carpeta.</p>

### Compilación
<p>Para compilar el programa utilizando GCC, se debe incluir Eigen utilizando la bandera -I y ligar la biblioteca de X11 con la bandera -l. Adicionalmente, se recomienda utilizar la opción de optimización de código de GCC -O3 para reducir los tiempos de renderizado, aunque aumenta el tiempo de compilación y el tamaño del ejecutable. El comando completo para compilar es el siguiente:</p>
<p><code>g++ proyecto1.cpp -o proyecto1  -I/path/to/eigen/ -lX11 -O3</code></p>

### Ejecución
<p>El programa recibe como argumento por consola el nombre del archivo con extensión .obj que se desea renderizar. Si no lo recibe o es incorrecto, termina automáticamente.</p>
  
### Instrucciones de uso
<p>Una vez que el programa inicia, se indica en la consola que se está cargando el modelo. Este proceso puede tardar dependiendo de la complejidad del mismo.</p>
<p>Posteriormente, aparecerá en la consola el mensaje “Renderizando.” y un porcentaje de progreso en el render. Además se podrá ver en la ventana desplegada cómo se va dibujando el modelo cara por cara. Si no aparece nada coherente, puede que la cámara no esté viendo el modelo o que esté dentro de él.</p>
<p>Una vez que el renderizado haya terminado, se puede interactuar con el modelo y la cámara usando las teclas como se muestra en la siguiente tabla:</p>
 <table>
  <tr>
    <th>Tecla(s)</th>
    <th>Función</th>
  </tr>
  <tr>
    <td>W y S</td>
    <td>Mover la cámara 0.5 unidades sobre el eje Y+ y Y-, respectivamente.</td>
  </tr>
  <tr>
    <td>A y D</td>
    <td>Mover la cámara 0.5 unidades sobre el eje X+ y Y-, respectivamente.</td>
  </tr>
  <tr>
    <td>A y D</td>
    <td>Mover la cámara 0.5 unidades sobre el eje X+ y Y-, respectivamente.</td>
  </tr>
  <tr>
    <td>Q y E</td>
    <td>Mover la cámara 0.5 unidades sobre el eje Z+ y Z-, respectivamente.</td>
  </tr>
  <tr>
    <td>I, J, K y L </td>
    <td>Rotar el modelo 10° sobre los ejes X+, Y+, X- o Y-, respectivamente.</td>
  </tr>
  <tr>
    <td>ESC</td>
    <td>Terminar el programa.</td>
  </tr>
  <tr>
    <td>Otra</td>
    <td>Cualquier otra tecla muestra estas instrucciones en la consola.</td>
  </tr>  
</table> 
<p>Una vez que se aplica una transformación sobre la cámara o sobre el modelo, se notifica la transformación realizada en la consola y se renderiza nuevamente el modelo, considerando dicha transformación. Cabe notar que cualquier interacción que se haga durante el proceso de renderizado se almacena en un buffer y se ejecutará una vez terminado el último render, por lo que recomiendo esperar a que termine el proceso de renderizado para interactuar nuevamente con el programa.</p>

### Flujo detallado de la implementación
<p>El programa renderiza cualquier archivo Wavefront .obj cuya maya esté triangulada y contenga normales. Para ello, me basé bastante en los tutoriales de Scratchapixel 2.0, en particular en la lección “Rasterization: a Practical Implementation” disponible en <a href="https://www.scratchapixel.com/lessons/3d-basic-rendering/rasterization-practical-implementation">esta liga</a>.</p>
<p>Para cargar los modelos, se escribió desde cero un procesador de archivos Wavefront .obj, el cual genera un vector que contiene todas los atributos de cada cara.</p>
<p>Posteriormente, ese vector se pasa a la función de renderizado, la cual despliega las imágenes utilizando el proceso de rasterización descrito en el tutorial de Scratchapixel 2.0 anteriormente mencionado.</p>
<p>Para la iluminación se asumen dos luces: una roja en la esquina superior izquierda del modelo y una azul en la esquina superior derecha del modelo. Estas luces permanecen estáticas, independientemente de las transformaciones que se apliquen al modelo.</p>
<p>Para el sombreado, se utilizó el modelo de iluminación de Phong. Sin embargo, para que funcione correctamente, el archivo .obj debe contener las normales suavizadas.</p>
<p>Para el texturizado, el programa ignora las UVs incluidas en el .obj y genera sus propias utilizando un mapeo esférico sobre el modelo. Sobre ellas se aplica el patrón de checker.</p>
<p>Finalmente, el resultado se despliega en una ventana del gestor de ventanas X, donde se puede desplazar la cámara virtual sobre cualquiera de los tres ejes y rotar el modelo sobre los ejes X y Y en coordenadas de plano de proyección, lo cual permite visualizar el modelo desde diferentes perspectivas.</p>
