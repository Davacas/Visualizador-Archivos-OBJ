/**
 * Proyecto #1 de la materia de graficación por computadora. PCIC. UNAM.
 * El programa carga cualquier archivo Wavefront .obj cuya maya sea triangulada e incluya normales,
 * le aplica una textura tipo checker, renderiza el modelo usando rasterización y lo despliega en 
 * una ventana del gestor X con un par de luces predeterminadas. Además, permite rotar el modelo 
 * sobre los ejes X y Y (en coordenadas de plano de proyección), así como desplazar la cámara por el mundo.
 * 
 * Requiere de las bibliotecas de desarrollo de X11 y Eigen 3.3.7 para funcionar.
 * Compilar con: g++ proyecto1.cpp -o proyecto1  -I/path/to/eigen/ -lX11 -O3
 * El programa muestra instrucciones de operación en la consola.
 * 
 * La implementación está basada en este tutorial de Scratchapixel 2.0:
 * https://www.scratchapixel.com/lessons/3d-basic-rendering/rasterization-practical-implementation
 */

#include <iostream>     //Entrada y salida estándar.
#include <X11/Xlib.h>   //Gestor de ventanas.
#ifdef Success          //Había conflicto entre macros de Eigen y X11.
    #undef Success
#endif
#include <Eigen/Dense>  //Para álgebra lineal.
#include <fstream>      //Manejo de archivos.
#include <vector>       //Vectores.

//Variables globales para facilitar la escritura del código.        
Display* display;   //Apuntador a la estructura de Display.
Window win;         //ID de la ventana.
GC gc;              //Contexto gráfico.
XEvent event;       //Para obtener eventos de teclado.
Colormap cm;        //Para hacer mapeos entre valores de color y pixeles.
int altoVen = 500;  //Altura de la ventana en pixeles.
int anchoVen = 500; //Anchura de la ventana en pixeles.
int planoCercano = 1;   //Profundidad del plano cercano.
int planoLejano = 1000; //Profundidad del plano lejano.

//Representación de un color en coordenadas normalizadas.
struct Color {
    float R;
    float G;
    float B;
};

//Representación de una cara (triángulo) del modelo.
struct Cara {
    Eigen::Vector4f vertice[3];     //Los tres vértices que componen la cara.
    Eigen::Vector2f coord_tex[3];   //Coordenadas de textura en cada vértice.
    Eigen::Vector4f normal[3];      //Normal en cada vértice.
    Eigen::Vector3f vertDisp[3];    //Vértice en coordenadas de dispositivo (con profundidad).
};

//Representación de una luz.
struct Luz {
    float Ka;           //Coeficiente ambiental.
    float Kd;           //Coeficiente difuso.
    float Ke;           //Coeficiente especular.
    float brillantez;   //Qué tan brillante es la luz (0 es lo más brillante).
    Eigen::Vector3f CAmbiental; //Color ambiental (se usan vectores para facilitar cálculos).
    Eigen::Vector3f CDifuso;    //Color difuso (se usan vectores para facilitar cálculos).
    Eigen::Vector3f CEspecular; //Color especular (se usan vectores para facilitar cálculos).
    Eigen::Vector4f posicion;   //Posición de la luz.
};

//Prototipos de funciones.
std::vector <Cara> OBJaModelo(std::string);
Eigen::Vector4f leerVec3DeCadena(std::string);
Cara obtenerPropiedadesCara(const std::string &, const std::vector<Eigen::Vector4f> &, const std::vector<Eigen::Vector4f> &);
void generarUVs(std::vector<Cara> &);
void configurarVentana();
void transformarModelo(std::vector<Cara> &, Eigen::Matrix4f);
void renderizar(const std::vector<Cara>&, const std::vector<Luz>&, Eigen::Matrix4f, float **);
Eigen::Vector3f verticeADispositivo(Eigen::Vector4f, Eigen::Matrix4f);
void dibujarMasCercano(Cara, const std::vector<Luz>&, Eigen::Matrix4f, float **, int, int, int, int);
float edgeFunction(Eigen::Vector3f, Eigen::Vector3f, Eigen::Vector3f);
Color obtenerColorPixel(Cara, const std::vector<Luz> &, Eigen::Vector4f, Eigen::Vector4f, Eigen::Vector2f);
void dibujarPuntoColor(int, int, float, float, float);
void limpiarVentana();

/** FUNCIÓN MAIN **/
int main(int argc, char* argv[]) {
    //Si no se ingresaron los argumentos, termina el programa.
    if (argc < 2) {
        std::cout << "Ingresa como argumento el archivo con extensión .obj a visualizar.\n";
        return -1;
    }

    bool continuar = true;          //Bandera que indica si continuar el programa o no.
    bool redibujar = true;          //Bandera que indica si hay que renderizar de nuevo o no.
    std::vector<Cara> modelo;       //Vector con las propiedades de todas las caras del modelo.
    std::vector<Luz> luces;         //Vector con las luces.
    float **bufferProf;             //Buffer (matriz) de profundidad.
    Eigen::Matrix4f camara;         //Matriz que representa la cámara en el espacio.
    Eigen::Matrix4f transformacion; //Matriz con las transformaciones que se aplicarán al modelo.
    Eigen::Matrix4f noTransformacion;//Matriz que representa ninguna transformación.
    
    //Inicialización de luces.
    Luz luz{1.0f, 1.0f, 1.0f, 5.0f,             //Ka, Kd, Ke, brillantez.     
            Eigen::Vector3f(0.0f, 0.0f, 0.0f),  //Color ambiental.
            Eigen::Vector3f(1.0f, 0.0f, 0.0f),  //Color especular.
            Eigen::Vector3f(1.0f, 1.0f, 1.0f),  //Color difuso.
            Eigen::Vector4f(-5.0, 5.0, 0.0, 1.0)};//Posición.

    Luz luz2{1.0f, 1.0f, 1.0f, 5.0f,            //Ka, Kd, Ke, brillantez.
            Eigen::Vector3f(0.0f, 0.0f, 0.0f),  //Color ambiental.
            Eigen::Vector3f(0.0f, 0.0f, 1.0f),  //Color especular.
            Eigen::Vector3f(1.0f, 1.0f, 1.0f),  //Color difuso.
            Eigen::Vector4f(5.0, 5.0, 0.0, 1.0)};//Posición.

    luces.push_back(luz);
    luces.push_back(luz2);    
    
    //Inicialización de matrices.
    camara <<   1.0, 0.0, 0.0, 0.0,
                0.0, 1.0, 0.0, 0.0,
                0.0, 0.0, 1.0, -5.0,
                0.0, 0.0, 0.0, 1.0;

    noTransformacion << 1.0, 0.0, 0.0, 0.0,
                        0.0, 1.0, 0.0, 0.0,
                        0.0, 0.0, 1.0, 0.0,
                        0.0, 0.0, 0.0, 1.0;
    transformacion = noTransformacion;

    //Creación del buffer de profundidad. 
    bufferProf = new float *[altoVen];
    for (int i = 0; i < altoVen; i++) {
        bufferProf[i] = new float[anchoVen]; 
    }

    //Carga del modelo y generación de UVs.    
    modelo = OBJaModelo(argv[1]);
    generarUVs(modelo);

    //Configuración inicial de la ventana.
    configurarVentana();
    
    std::cout << "\nINSTRUCCIONES DE USO DEL PROGRAMA:\n"; 
    std::cout << " - Presiona las teclas W, S, A o D para mover la cámara hacia arriba,\n"; 
    std::cout << "   hacia abajo, a la izquierda o a la derecha, respectivamente.\n";
    std::cout << " - Presiona la tecla Q para acercar la cámara al modelo.\n";
    std::cout << " - Presiona la tecla E para alejar la cámara al modelo.\n";
    std::cout << " - Presiona las teclas I, J, K o L para rotar el modelo\n";
    std::cout << "   10° sobre los ejes X+, Y+, X- y Y-, respectivamente.\n";
    std::cout << " - Presiona la tecla ESC para terminar el programa.\n";
    std::cout << " - Presiona cualquier otra tecla para desplegar estas instrucciones.\n";
    
    //Bucle para capturar eventos y redibujar la pantalla
    while (continuar) {
        if (redibujar) {                                    //Si hay que renderizar nuevamente.
            std::cout << "La cámara está en la posición (" << -camara(0, 3) << ", " << -camara(1, 3) << ", " << -camara(2, 3)<< ").\n";
            limpiarVentana();                               //Se pinta la ventana de negro.
            transformarModelo(modelo, transformacion);      //Se aplican transformaciones al modelo.
            renderizar(modelo, luces, camara, bufferProf);  //Se renderiza el modelo.
            transformacion = noTransformacion;              //Se reinicia la matriz de transformaciones.
        }
        redibujar = true;
        
        //Captura y procesamiento de evento de teclado.
        XNextEvent(display, &event);
        if (event.type == KeyPress) {
            switch (event.xkey.keycode) {    
                case 9:     //ESC = Salir del programa.
                    continuar = false;
                    break;
                case 24:    //Q = Mover cámara hacia afuera.
                    std::cout << "\nSe desplazó la cámara 0.5 unidades sobre el eje Z.\n";
                    camara(2, 3) -= 0.5f; 
                    break;
                case 26:    //E = Mover cámara hacia el fondo.
                    std::cout << "\nSe desplazó la cámara -0.5 unidades sobre el eje Z.\n";
                    camara(2, 3) += 0.5f;
                    break;
                case 25:    //W = Mover cámara hacia arriba.
                    std::cout << "\nSe desplazó la cámara 0.5 unidades sobre el eje Y.\n";
                    camara(1, 3) -= 0.5f; 
                    break;
                case 38:    //A = Mover cámara hacia la izquierda.
                    std::cout << "\nSe desplazó la cámara -0.5 unidades sobre el eje X.\n";
                    camara(0, 3) += 0.5f; 
                    break;
                case 39:    //S = Mover cámara hacia abajo.
                    std::cout << "\nSe desplazó la cámara -0.5 unidades sobre el eje Y.\n";
                    camara(1, 3) += 0.5f; 
                    break;
                case 40:    //D = Mover cámara hacia la derecha.
                    std::cout << "\nSe desplazó la cámara 0.5 unidades sobre el eje X.\n";
                    camara(0, 3) -= 0.5f; 
                    break;
                case 31:    //I = Rotar modelo sobre X+
                    std::cout << "\nSe rotó el modelo 10° sobre el eje X+.\n";
                    transformacion(1, 1) = cos(10*(M_PI/180));
                    transformacion(1, 2) = sin(10*(M_PI/180)); 
                    transformacion(2, 1) = -sin(10*(M_PI/180));
                    transformacion(2, 2) = cos(10*(M_PI/180));
                    break;
                case 44:    //J = Rotar modelo sobre Y+
                    std::cout << "\nSe rotó el modelo 10° sobre el eje Y+.\n";
                    transformacion(0, 0) = cos(10*(M_PI/180));
                    transformacion(0, 2) = sin(10*(M_PI/180)); 
                    transformacion(2, 0) = -sin(10*(M_PI/180));
                    transformacion(2, 2) = cos(10*(M_PI/180));
                    break;
                case 45:    //K = Rotar modelo sobre X-
                    std::cout << "\nSe rotó el modelo 10° sobre el eje X-.\n";
                    transformacion(1, 1) = cos(-10*(M_PI/180));
                    transformacion(1, 2) = sin(-10*(M_PI/180)); 
                    transformacion(2, 1) = -sin(-10*(M_PI/180));
                    transformacion(2, 2) = cos(-10*(M_PI/180));
                    break;
                case 46:    //posRelLuz = Rotar modelo sobre Y-
                    std::cout << "\nSe rotó el modelo 10° sobre el eje Y-.\n";
                    transformacion(0, 0) = cos(-10*(M_PI/180));
                    transformacion(0, 2) = sin(-10*(M_PI/180)); 
                    transformacion(2, 0) = -sin(-10*(M_PI/180));
                    transformacion(2, 2) = cos(-10*(M_PI/180));
                    break;
                default:
                    std::cout << "\nINSTRUCCIONES DE USO DEL PROGRAMA:\n"; 
                    std::cout << " - Presiona las teclas W, S, A o D para mover la cámara hacia arriba,\n"; 
                    std::cout << "   hacia abajo, a la izquierda o a la derecha, respectivamente.\n";
                    std::cout << " - Presiona la tecla Q para acercar la cámara al modelo.\n";
                    std::cout << " - Presiona la tecla E para alejar la cámara al modelo.\n";
                    std::cout << " - Presiona las teclas I, J, K o L para rotar el modelo\n";
                    std::cout << "   10° sobre los ejes X+, Y+, X- y Y-, respectivamente.\n";
                    std::cout << " - Presiona la tecla ESC para terminar el programa.\n";
                    std::cout << " - Presiona cualquier otra tecla para desplegar estas instrucciones.\n";
                    redibujar = false;
                    break;
            }
        }
    }

    //Se elimina el buffer de profundidad.
    for (int i = 0; i < altoVen; i++) {
        delete [] bufferProf[i];
    }
    delete [] bufferProf;

    XCloseDisplay(display);

    return 0;    
}
/* TERMINA FUNCIÓN MAIN */

//Se abre el OBJ nombre_archivo y se regresa un vector con las propiedades de las caras.
std::vector <Cara> OBJaModelo(std::string nombre_archivo) {
    std::string linea;      //Linea leída del archivo.
    std::vector<Eigen::Vector4f> vertices;  //Vector con los vértices leídos.
    std::vector<Eigen::Vector2f> coord_tex; //Vector con las coordenadas UV (de textura) leídas.
    std::vector<Eigen::Vector4f> normales;  //Vector con las normales leídas.
    std::ifstream archivo;   //Archivo OBJ con los puntos.
    std::vector <Cara> caras;

    //Apertura y validación de apertura del archivo.
    archivo.open(nombre_archivo, std::ios::in); 
    if (!archivo.is_open()) {
        std::cout << "No se pudo abrir el archivo '" << nombre_archivo << "'." << std::endl;
        std::cout << "Probablemente el nombre y/o extensión es incorrecto." << std::endl;
        exit(-1);
    }
    std::cout << "Cargando el archivo '" << nombre_archivo << "'.\n";
    
    while (std::getline(archivo, linea)) {              //Mientras haya líneas por leer, se toma una.
        if (linea[0] == 'v') {                          //Si la línea empieza con "v"
            if (linea[1] == 'n') {                      //Si la línea es una normal, se obtiene su valor.
                normales.push_back(leerVec3DeCadena(linea));
            }
            else if (linea[1] == ' ') {                 //Si la línea es un vértice, se obtiene su valor.
                vertices.push_back(leerVec3DeCadena(linea));
            }
        }
        else if(linea[0] == 'f') {   //Si la línea es una cara, se obtienen sus propiedades.
            caras.push_back(obtenerPropiedadesCara(linea, vertices, normales)); 
        }
    }
    archivo.close();
    std::cout << "El modelo se cargó con éxito.\n";
    return caras;
}

//Se convierte una línea (cadena) del archivo a un punto en 3D con coordenadas homogéneas.
//Con esto se leen vértices y normales en el OBJ.
Eigen::Vector4f leerVec3DeCadena(std::string cadena) {
    Eigen::Vector4f punto;  //Punto recuperado.
    std::string numero;     //Cadena que se va a convertir a float.
    char caracter;          //Caracter leído.
    int coord = 0;          //Coordenada (X, Y, Z) que se está leyendo
    
    if (std::sscanf(cadena.c_str(), "vn %f %f %f", &punto[0], &punto[1], &punto[2]) != 3) {
        std::sscanf(cadena.c_str(), "v %f %f %f", &punto[0], &punto[1], &punto[2]);
    }
    punto[3] = 1.0f;   //Se agrega la coordenada homogénea.

    return punto;
}

//Se toma una línea (cadena) del OBJ que representa una cara, un vector de vértices, uno de UVs y uno normales
//y se regresa una estructura con las propiedades correspondientes a esa cara.
Cara obtenerPropiedadesCara(const std::string &cadena, const std::vector<Eigen::Vector4f> &vertices, const std::vector<Eigen::Vector4f> &normales) {
    Cara cara;              //Contiene las propiedades de la cara obtenida.
    int i_vert = 0;              //Índice que representa el número de vértice actual.
    int iv = 0, it = 0, in = 0;     //Índice de la coordenada de vértice, textura y normal.
    std::vector<int> indices;   //Vector con los índices obtenidos por cada línea.
    std::stringstream cadena_aux(cadena);   //Cadena auxiliar para particionar la original por tokens.
    std::string subcadena;      //Cadena obtenida de particionar cadena_aux;

    getline(cadena_aux, subcadena, ' ');            //Se ignora la primer subcadena, que es la letra F.
    while(getline(cadena_aux, subcadena, ' ')) {    //Se tokeniza la cadena por espacios, para obtener las propiedades de cada vértice.
        if (std::sscanf(subcadena.c_str(), "%i/%i/%i", &iv, &it, &in) != 3) {
            std::sscanf(subcadena.c_str(), "%i//%i", &iv, &in);
        }

        if (iv < 0) {
            iv = vertices.size() + iv+1;
        }
        if (in < 0) {
            in = normales.size() + in+1;
        }

        cara.vertice[i_vert] = vertices[iv-1];
        cara.normal[i_vert] = normales[in-1].normalized();
        
        i_vert ++;
        indices.clear();  
    } 
    return cara;
}

//Se toma el vector de caras que representa al modelo y se generan sus coordenadas de textura UV utilizando un mapeo esférico.
void generarUVs(std::vector<Cara> &modelo) {
    Eigen::Vector4f centroideModelo(0,0,0,0);//Centroide del modelo.
    Eigen::Vector4f vecCentVert;             //Vector que va del centro del modelo al vértice.

    //Se calcula el centroide del modelo como el promedio de todos los vértices.
    for (int i = 0; i < modelo.size(); i++) {
        centroideModelo += modelo[i].vertice[0];
        centroideModelo += modelo[i].vertice[1];
        centroideModelo += modelo[i].vertice[2];
    }
    centroideModelo /= modelo.size() * 3;
    
    //Se generan las UVs en cada vértice haciendo un mapeo esférico.
    for (int i = 0; i < modelo.size(); i++) {
        for (int j = 0; j < 3; j ++) {
            vecCentVert = (centroideModelo - modelo[i].vertice[j]).normalized();
            modelo[i].coord_tex[j].x() = 0.5 + atan2(vecCentVert.z(), vecCentVert.x()) / (2*M_PI);
            modelo[i].coord_tex[j].y() = 0.5 - asin(vecCentVert.y()) / M_PI;
        }
    }
}

//Función para configurar la ventana y los eventos de teclado de X11.
void configurarVentana() {
    //Apertura del display.
    display = XOpenDisplay(0);
    if (display == NULL) {
        std::cout << "No se pudo conectar a X server." << std::endl;
        exit(-1);
    }

    //Creación de la ventana.
    win = XCreateSimpleWindow(display, DefaultRootWindow(display), 0, 0, altoVen, anchoVen, 0, WhitePixel(display, DefaultScreen(display)), BlackPixel(display, DefaultScreen(display)));

    //Se asigna un nombre a la ventana generada.
    XStoreName(display, win, "PROYECTO 1");

    //Mostrar la ventana en pantalla.
    XMapWindow(display, win);

    //Solicitar a X server que mande notificaciones de la ventana activa.
    XSelectInput(display, win, KeyPressMask | StructureNotifyMask);    

    //Obtención del colormap.
    cm = DefaultColormap(display, DefaultScreen(display));

    //Crear el contexto gráfico con valores por defecto (0, 0).
    gc = XCreateGC(display, win, 0, 0);

    //Se espera el evento MapNotify para poder empezar el dibujo.
    for(;;) {
        XEvent e;
	    XNextEvent(display, &e);
	    if (e.type == MapNotify)
		    break;
    }

    //Se indica que se va a dibujar siempre puntos blancos.
    XSetForeground(display, gc, WhitePixel(display, DefaultScreen(display)));

    return;
}

//Se aplica la matriz "transformacion" al vector de caras "modelo" pasados como argumento.
void transformarModelo(std::vector<Cara> &modelo, Eigen::Matrix4f transformacion) {
    //Por cada cara en el modelo, se aplica la transformación a cada vértice y normal.
    Eigen::Vector4f uv;
    Eigen::Vector4f uvRot;
    for (int i = 0; i < modelo.size(); i++) {
        modelo[i].vertice[0] = transformacion * modelo[i].vertice[0];
        modelo[i].vertice[1] = transformacion * modelo[i].vertice[1];
        modelo[i].vertice[2] = transformacion * modelo[i].vertice[2];
        modelo[i].normal[0] = transformacion * modelo[i].normal[0];
        modelo[i].normal[1] = transformacion * modelo[i].normal[1];
        modelo[i].normal[2] = transformacion * modelo[i].normal[2];
    }
}

//Se toma el modelo, las luces, la cámara, el buffer de proyección y se realiza el proceso de renderizado.
void renderizar(const std::vector<Cara> &modelo, const std::vector<Luz> &luces, Eigen::Matrix4f camara, float **bufferProf) {
    float xmin, ymin, xmax, ymax;
    int xi, yi, xf, yf;

    //Se rellena el buffer de profundidad con el valor del plano lejano.
    for (int i = 0; i < altoVen; i++) {
        for (int j = 0; j < anchoVen; j++)
        bufferProf[i][j] = planoLejano;
    }

    //Se analiza cada una de las caras del modelo.
    for (int i = 0; i < modelo.size(); ++i) {
        std::cout << "Renderizando. " << int((i+1) / float(modelo.size())*100) << "% completo.\r";
        Cara cara = modelo[i];

        //Se obtienen los vértices de la cara en coordenadas de dispositivo.
        cara.vertDisp[0] = verticeADispositivo(cara.vertice[0], camara);
        cara.vertDisp[1] = verticeADispositivo(cara.vertice[1], camara);
        cara.vertDisp[2] = verticeADispositivo(cara.vertice[2], camara);

        //Se guarda el valor invertido de la coordenada Z para saber la profundidad de la cara.
        cara.vertDisp[0].z() = 1 / cara.vertDisp[0].z();
        cara.vertDisp[1].z() = 1 / cara.vertDisp[1].z();
        cara.vertDisp[2].z() = 1 / cara.vertDisp[2].z();

        //Se obtienen las coordenadas X y Y más lejanas de los vértices.
        xmin = std::min(cara.vertDisp[0].x(), std::min(cara.vertDisp[1].x(), cara.vertDisp[2].x()));
        ymin = std::min(cara.vertDisp[0].y(), std::min(cara.vertDisp[1].y(), cara.vertDisp[2].y()));
        xmax = std::max(cara.vertDisp[0].x(), std::max(cara.vertDisp[1].x(), cara.vertDisp[2].x()));
        ymax = std::max(cara.vertDisp[0].y(), std::max(cara.vertDisp[1].y(), cara.vertDisp[2].y()));
        
        //Se verifica si la cara entra en la ventana (si se va a dibujar o no)
        if (xmin > anchoVen - 1 || ymin > altoVen - 1 || xmax < 0  || ymax < 0) {
            continue;
        }
        else {
            //Se obtienen las esquinas de un cuadrilátero que engloba la cara.
            xi = std::max(0, int(std::floor(xmin)));
            yi = std::max(0, int(std::floor(ymin)));
            xf = std::min(anchoVen-1, int(std::floor(xmax)));
            yf = std::min(altoVen-1, int(std::floor(ymax)));
            //Se manda a dibujar la cara con las coordenadas recién obtenidas.
            dibujarMasCercano(cara, luces, camara, bufferProf, xi, yi, xf, yf);
        }
    }
    std::cout << std::endl;
}

//Se toma un vértice en coordenadas de mundo, una cámara y regresa el punto en coordenadas de dispositivo.
//Basado en este tutorial: https://www.scratchapixel.com/lessons/3d-basic-rendering/computing-pixel-coordinates-of-3d-point/mathematics-computing-2d-coordinates-of-3d-points
Eigen::Vector3f verticeADispositivo(Eigen::Vector4f vertice, Eigen::Matrix4f camara) {
    Eigen::Vector4f verticeCamara;
    Eigen::Vector2f verticePlanoProy;
    Eigen::Vector2f verticeNDC;
    Eigen::Vector3f verticeDispositivo;
    
    //Se obtiene el vértice en coordenadas de cámara.
    verticeCamara = camara * vertice;
    
    //Se proyecta ese vértice en el plano de proyección.
    verticePlanoProy.x() = planoCercano * verticeCamara.x() / -verticeCamara.z();
    verticePlanoProy.y() = planoCercano * verticeCamara.y() / -verticeCamara.z();
    
    //Proyección a coordenadas de dispositivo normalizadas (NDC).
    verticeNDC.x() = 2 * verticePlanoProy.x();
    verticeNDC.y() = 2 * verticePlanoProy.y();
    
    //Obtención del vértice en coordenadas del dispositivo.
    verticeDispositivo.x() = (verticeNDC.x() + 1) / 2 * anchoVen;
    verticeDispositivo.y() = (1 - verticeNDC.y()) / 2 * altoVen;
    verticeDispositivo.z() = -verticeCamara.z();

    return verticeDispositivo;
}

//Se toma una cara, luces, cámara, buffer de profundidad y coordenadas de la cara y se dibuja esa cara si es la más cercana.
void dibujarMasCercano(Cara cara, const std::vector<Luz> &luces, Eigen::Matrix4f camara, float **bufferProf, int xi, int yi, int xf, int yf) {
    float w0, w1, w2;                   //Coordenadas baricéntricas de la cara.
    Eigen::Vector3f centroPixel;        //Coordenadas del centro del pixel.
    Eigen::Vector4f v0Cam, v1Cam, v2Cam;//Vértices de la cara en coordenadas de cámara.
    Eigen::Vector4f pixelCam;           //Pixel en coordenadas de cámara.
    Eigen::Vector4f normalInterp;       //Normal de la cara interpolada para este pixel.
    Eigen::Vector2f uv0, uv1, uv2;      //Coordenadas UV en coordenadas de cámara..
    Eigen::Vector2f uvInterp;           //Coordenadas UV interpoladas en el punto actual.
    Color colorPixel;                   //Color final que tendrá este pixel.
    float pxCam, pyCam;                 //Coordenadas del pixel en en coordenadas de cámara.
    float z;                            //Valor de profundidad de la cara.
    
    //Precálculo del área de la cara.
    float area = edgeFunction(cara.vertDisp[0], cara.vertDisp[1], cara.vertDisp[2]);
    
    //Rasterización de la cara. Se verifica cada pixel (punto) que está en la cara.
    for (int y = yi; y <= yf; y++) {
        for (int x = xi; x <= xf; x++) {
            //Se obtiene el centro del pixel.
            centroPixel = Eigen::Vector3f(x + 0.5, y + 0.5, 0);
            
            //Coordenadas baricéntricas del pixel actual.
            w0 = edgeFunction(cara.vertDisp[1], cara.vertDisp[2], centroPixel);
            w1 = edgeFunction(cara.vertDisp[2], cara.vertDisp[0], centroPixel);
            w2 = edgeFunction(cara.vertDisp[0], cara.vertDisp[1], centroPixel);

            //Si el pixel (coordenadas baricéntricas) está dentro de la cara, se verifica su profundidad.
            if (w0 >= 0 && w1 >= 0 && w2 >= 0) {
                //Normalización de las coordenadas baricéntricas para que sumen 1.
                w0 /= area;
                w1 /= area;
                w2 /= area;

                //Obtención de la profundidad de esta cara.
                z = 1/(cara.vertDisp[0].z() * w0 + cara.vertDisp[1].z() * w1 + cara.vertDisp[2].z() * w2);
                
                //Si esta profundidad es menor a la que hay en el buffer actualmente, se dibuja el punto.
                if (z < bufferProf[x][y]) {
                    //Se actualizada el valor mínimo de profundidad en el buffer de profundidad.
                    bufferProf[x][y] = z;                

                    //Se obtienen este punto en coordenadas de cámara.
                    v0Cam = camara * cara.vertice[0]; 
                    v1Cam = camara * cara.vertice[1]; 
                    v2Cam = camara * cara.vertice[2];
                    pxCam = w0 * (v0Cam.x()/-v0Cam.z()) + w1 * (v1Cam.x()/-v1Cam.z()) + w2 * (v2Cam.x()/-v2Cam.z());
                    pyCam = (v0Cam.y()/-v0Cam.z()) * w0 + (v1Cam.y()/-v1Cam.z()) * w1 + (v2Cam.y()/-v2Cam.z()) * w2;
                    pixelCam = Eigen::Vector4f(pxCam * z, pyCam * z, -z, 1);

                    //Se interpola la normal en este punto.
                    normalInterp = (w0 * cara.normal[0] + w1 * cara.normal[1] + w2 * cara.normal[2]).normalized();
                    
                    //Se interpola la coordenada UV para el punto actual.
                    uv0 = cara.coord_tex[0] * cara.vertDisp[0].z();
                    uv1 = cara.coord_tex[1] * cara.vertDisp[0].z();
                    uv2 = cara.coord_tex[2] * cara.vertDisp[0].z(); 
                    uvInterp = (uv0 * w0 + uv1 * w1 + uv2 * w2) * z;  

                    //Se obtiene el color del pixel a partir de los valores recién calculados.
                    colorPixel = obtenerColorPixel(cara, luces, pixelCam, normalInterp, uvInterp);

                    //Se dibuja el pixel actual con el color obtenido.
                    dibujarPuntoColor(x, y, colorPixel.R, colorPixel.G, colorPixel.B);
                }
            }
        }
    }
}

//Implementación de "Edge Function" de Juan Pineda. Regresa una coordenada baricéntrica a partir de tres vértices.
float edgeFunction(Eigen::Vector3f a, Eigen::Vector3f b, Eigen::Vector3f c) { 
    return (c[0] - a[0]) * (b[1] - a[1]) - (c[1] - a[1]) * (b[0] - a[0]); 
}

//Se obtiene el color del pixel actual, considerando luz y textura tipo checker. 
//La iluminación se calcula con el modelo de Phong y la textura se genera proceduralmente.
//Función basada en esta implementación: http://www.cs.toronto.edu/~jacobson/phong-demo/
Color obtenerColorPixel(Cara cara, const std::vector<Luz> &luces, Eigen::Vector4f pixelCam, Eigen::Vector4f normalInterp, Eigen::Vector2f uv) {
    Eigen::Vector4f posRelLuz;      //Posición de la luz con respecto al punto.
    Eigen::Vector4f vecReflexion;   //Vector de reflexión de la luz.
    Eigen::Vector4f vecCamara;      //Vector del punto a la cámara.
    Eigen::Vector3f color;          //Color final
    float lambertian;       //Lambertiano calculado.
    float specular;         //Componente especular calculada.
    float specAngle;        //Ángulo de la componente especular.
    int tamChecker = 10;    //Valor de cada cuadro del checker (entre más grande, más pequeño el cuadro).
    float checker;          //Valor generado por el checker en este punto. 
    float intensidadChecker;//Intensidad del checker en este punto.       

    //Se hace el cálculo por cada luz.
    for (int i = 0; i < luces.size(); i++) {
        //Se calcula la posición de la luz con respecto al punto deseado.
        posRelLuz = (luces[i].posicion - pixelCam).normalized();

        //Se calcula el Lambertiano.
        lambertian = std::max(normalInterp.dot(posRelLuz), 0.0f);
        specular = 0.0f;

        //Si el lambertiano es mayor a 0.
        if(lambertian > 0.0f) {
            vecReflexion = -posRelLuz -2.0 * normalInterp.dot(-posRelLuz) * normalInterp;   //Se calcula el vector de reflexión.
            vecCamara = -pixelCam.normalized();                     //Se calcula el vector del punto a la cámara.
            specAngle = std::max(vecReflexion.dot(vecCamara), 0.0f);//Se calcula el ángulo de la componente especular.
            specular = std::pow(specAngle, luces[i].brillantez);    //Se calcula la componente especular.
        }

        //Si es la primera luz, se guarda la componente de luz obtenida.
        if (i == 0) {
            color= (luces[i].Ka * luces[i].CAmbiental +
                    luces[i].Kd * lambertian * luces[i].CDifuso +
                    luces[i].Ke * specular * luces[i].CEspecular);
        }
        //Si no, se acumula la componente de luz obtenida.
        else {
            color+=(luces[i].Ka * luces[i].CAmbiental +
                    luces[i].Kd * lambertian * luces[i].CDifuso +
                    luces[i].Ke * specular * luces[i].CEspecular);
        }
    }
    
    //Obtención del valor de la textura en este punto.
    checker = (fmod(uv.x() * tamChecker, 1.0) > 0.5) ^ (fmod(uv.y() * tamChecker, 1.0) < 0.5);
    intensidadChecker = 0.2 * (1 - checker) + 0.8 * checker;

    //Se aplica el valor del checker al color obtenido en este punto.
    color *= intensidadChecker;

    return Color{color[0], color[1], color[2]};
}

//Se colorea un pixel con coordenadas X y Y con sus componentes R, G y B normalizadas.
void dibujarPuntoColor(int x, int y, float R, float G, float B) {
    char colorString[26];
    XColor color;

    //Se obtiene el color en el formato de X11.
    sprintf(colorString, "RGBi:%6.4f/%6.4f/%6.4f", R, G, B);
    XAllocNamedColor(display, cm, colorString, &color, &color);

    //Se dibuja el pixel con el color obtenido.
    XSetForeground(display, gc, color.pixel);
    XDrawPoint(display, win, gc, x, y);

    return;
}

//Se pinta toda la ventana de negro.
void limpiarVentana() {
    XSetForeground(display, gc, BlackPixel(display, DefaultScreen(display)));
    
    for (int x = 0; x <= anchoVen; x++)
        for (int y = 0; y <= altoVen; y++)
            XDrawPoint(display, win, gc, x, y);
    
    XSetForeground(display, gc, WhitePixel(display, DefaultScreen(display)));

    return;
}