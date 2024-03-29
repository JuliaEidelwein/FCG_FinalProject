#version 330 core

// Atributos de fragmentos recebidos como entrada ("in") pelo Fragment Shader.
// Neste exemplo, este atributo foi gerado pelo rasterizador como a
// interpolação da cor de cada vértice, definidas em "shader_vertex.glsl" e
// "main.cpp".
in vec4 position_world;
in vec4 normal;

// Posição do vértice atual no sistema de coordenadas local do modelo.
in vec4 position_model;

// Coordenadas de textura obtidas do arquivo OBJ (se existirem!)
in vec2 texcoords;

in vec4 interpColor;

vec3 newInterpColor = vec3(interpColor);

// Matrizes computadas no código C++ e enviadas para a GPU
uniform mat4 model;
uniform mat4 view;
uniform mat4 projection;

// Identificador que define qual objeto está sendo desenhado no momento
#define SPHERE 0
#define BUNNY  1
#define PLANE  2
#define BLOCKADE 3
#define BUS 4
#define COW 5

uniform int object_id;

// Parâmetros da axis-aligned bounding box (AABB) do modelo
uniform vec4 bbox_min;
uniform vec4 bbox_max;

// Variáveis para acesso das imagens de textura
uniform sampler2D TextureImage0;
uniform sampler2D TextureImage1;
uniform sampler2D TextureImage2;

// O valor de saída ("out") de um Fragment Shader é a cor final do fragmento.
out vec3 color;

// Constantes
#define M_PI   3.14159265358979323846
#define M_PI_2 1.57079632679489661923

void main()
{
    // Obtemos a posição da câmera utilizando a inversa da matriz que define o
    // sistema de coordenadas da câmera.
    vec4 origin = vec4(0.0, 0.0, 0.0, 1.0);
    vec4 camera_position = inverse(view) * origin;

    // O fragmento atual é coberto por um ponto que percente à superfície de um
    // dos objetos virtuais da cena. Este ponto, p, possui uma posição no
    // sistema de coordenadas global (World coordinates). Esta posição é obtida
    // através da interpolação, feita pelo rasterizador, da posição de cada
    // vértice.
    vec4 p = position_world;

    // Normal do fragmento atual, interpolada pelo rasterizador a partir das
    // normais de cada vértice.
    vec4 n = normalize(normal);

    // Vetor que define o sentido da fonte de luz em relação ao ponto atual.
    vec4 l = normalize(vec4(1.0,1.0,-1.0,0.0));

    // Vetor que define o sentido da câmera em relação ao ponto atual.
    vec4 v = normalize(camera_position - p);

    // Vetor que define o sentido da reflexão especular ideal.
    vec4 r = -l + 2 * n * dot(n, l);

    // Coordenadas de textura U e V
    float U = 0.0;
    float V = 0.0;

    // Parâmetros que definem as propriedades espectrais da superfície
    vec3 Kd = newInterpColor; // Refletância difusa
    vec3 Ks = vec3(0.0f, 0.0f, 0.0f); // Refletância especular
    vec3 Ka = Kd * 2; // Refletância ambiente
    float q = 1; // Expoente especular para o modelo de iluminação de Phong

    if ( object_id == SPHERE )
    {
        vec4 bbox_center = (bbox_min + bbox_max) / 2.0;

        vec4 plinha = bbox_center + length(bbox_max - bbox_center)*((position_model - bbox_center)/length(position_model - bbox_center));
        vec4 pvec = plinha - bbox_center;
        V = asin(pvec.y/length(pvec));
        U = atan((pvec.x),(pvec.z));

        V = ((M_PI/2) + V)/M_PI;
        U = (M_PI + U)/(2*M_PI);
    }
    else if (object_id == BLOCKADE)
    {
        float minx = bbox_min.x;
        float maxx = bbox_max.x;

        float miny = bbox_min.y;
        float maxy = bbox_max.y;

        float minz = bbox_min.z;
        float maxz = bbox_max.z;

        U = (position_model.x - minx)/(maxx - minx);
        V = (position_model.y - miny)/(maxy - miny);
        Kd = texture(TextureImage2, vec2(U,V)).rgb;
        Ka = Kd * 0.5;
        Ks = vec3(0.8, 0.8, 0.8);
        q = 20;
    }
    else if (object_id == COW)
    {
        Kd = vec3(0.8, 0.0, 0.0);
        Ks = vec3(0.8, 0.8, 0.8);
        Ka = Kd / 2;
        q = 80.0;
    } else if(object_id == BUS) {
        Kd = vec3(0.6, 0.6, 0.6);
        Ks = vec3(0.8, 0.8, 0.8);
        Ka = Kd / 2;
        q = 20.0;
    }

    // Espectro da fonte de iluminação
    vec3 I = vec3(1.0,1.0,1.0);

    // Espectro da luz ambiente
    vec3 Ia = vec3(0.5, 0.5, 0.5);

    // Termo difuso utilizando a lei dos cossenos de Lambert
    vec3 lambert_diffuse_term = Kd * I * max(0, dot(n, v));

    // Termo ambiente
    vec3 ambient_term = Ka * Ia;

    // Termo especular utilizando o modelo de iluminação de Phong
    vec3 phong_specular_term = Ks * I * pow(max(0, dot(r, v)), q);

    color = lambert_diffuse_term + ambient_term + phong_specular_term;

    color = pow(color, vec3(1.0,1.0,1.0)/2.2);
}
