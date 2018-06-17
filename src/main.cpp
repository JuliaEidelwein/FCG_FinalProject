#define STB_IMAGE_IMPLEMENTATION
#define TINYOBJLOADER_IMPLEMENTATION
#include <cmath>
#include <cstdio>
#include <cstdlib>

// Headers abaixo são específicos de C++
#include <map>
#include <stack>
#include <string>
#include <limits>
#include <vector>
#include <fstream>
#include <sstream>
#include <stdexcept>
#include <algorithm>

// Headers das bibliotecas OpenGL
#include <glad/glad.h>   // Criação de contexto OpenGL 3.3
#include <GLFW/glfw3.h>  // Criação de janelas do sistema operacional

// Headers da biblioteca GLM: criação de matrizes e vetores.
#include <glm/mat4x4.hpp>
#include <glm/vec4.hpp>
#include <glm/gtc/type_ptr.hpp>

// Headers da biblioteca para carregar modelos obj
#include <tiny_obj_loader.h>
#include <stb_image.h>

// Headers locais, definidos na pasta "include/"
#include "utils.h"
#include "matrices.h"

#define PI 3.141592f

// Declaração de funções utilizadas para pilha de matrizes de modelagem.
void PushMatrix(glm::mat4 M);
void PopMatrix(glm::mat4& M);

// Declaração de várias funções utilizadas em main().  Essas estão definidas
// logo após a definição de main() neste arquivo.
void DrawCube(GLint render_as_black_uniform); // Desenha um cubo
void DrawPlane(GLint render_as_black_uniform);
GLuint BuildTriangles(); // Constrói triângulos para renderização
GLuint LoadShader_Vertex(const char* filename);   // Carrega um vertex shader
GLuint LoadShader_Fragment(const char* filename); // Carrega um fragment shader
void LoadShader(const char* filename, GLuint shader_id); // Função utilizada pelas duas acima
GLuint CreateGpuProgram(GLuint vertex_shader_id, GLuint fragment_shader_id); // Cria um programa de GPU

// Declaração de funções auxiliares para renderizar texto dentro da janela
// OpenGL. Estas funções estão definidas no arquivo "textrendering.cpp".
void TextRendering_Init();
float TextRendering_LineHeight(GLFWwindow* window);
float TextRendering_CharWidth(GLFWwindow* window);
void TextRendering_PrintString(GLFWwindow* window, const std::string &str, float x, float y, float scale = 1.0f);
void TextRendering_PrintMatrix(GLFWwindow* window, glm::mat4 M, float x, float y, float scale = 1.0f);
void TextRendering_PrintVector(GLFWwindow* window, glm::vec4 v, float x, float y, float scale = 1.0f);
void TextRendering_PrintMatrixVectorProduct(GLFWwindow* window, glm::mat4 M, glm::vec4 v, float x, float y, float scale = 1.0f);
void TextRendering_PrintMatrixVectorProductDivW(GLFWwindow* window, glm::mat4 M, glm::vec4 v, float x, float y, float scale = 1.0f);

// Funções abaixo renderizam como texto na janela OpenGL algumas matrizes e
// outras informações do programa. Definidas após main().
void TextRendering_ShowModelViewProjection(GLFWwindow* window, glm::mat4 projection, glm::mat4 view, glm::mat4 model, glm::vec4 p_model);
void TextRendering_ShowEulerAngles(GLFWwindow* window);
void TextRendering_ShowProjection(GLFWwindow* window);
void TextRendering_ShowFramesPerSecond(GLFWwindow* window);

// Funções callback para comunicação com o sistema operacional e interação do
// usuário. Veja mais comentários nas definições das mesmas, abaixo.
void FramebufferSizeCallback(GLFWwindow* window, int width, int height);
void ErrorCallback(int error, const char* description);
void KeyCallback(GLFWwindow* window, int key, int scancode, int action, int mode);
void MouseButtonCallback(GLFWwindow* window, int button, int action, int mods);
void CursorPosCallback(GLFWwindow* window, double xpos, double ypos);
void ScrollCallback(GLFWwindow* window, double xoffset, double yoffset);

bool PlayerFloorColision(float floorY, float playerLowerY);
void BuildCharacter(double currentTime, GLint model_uniform, GLint render_as_black_uniforms, GLuint program_id);

// Estrutura que representa um modelo geométrico carregado a partir de um
// arquivo ".obj". Veja https://en.wikipedia.org/wiki/Wavefront_.obj_file .
struct ObjModel
	{
	    tinyobj::attrib_t                 attrib;
	    std::vector<tinyobj::shape_t>     shapes;
	    std::vector<tinyobj::material_t>  materials;

	    // Este construtor lê o modelo de um arquivo utilizando a biblioteca tinyobjloader.
	    // Veja: https://github.com/syoyo/tinyobjloader
	    ObjModel(const char* filename, const char* basepath = NULL, bool triangulate = true)
	    {
	        printf("Carregando modelo \"%s\"... ", filename);

	        std::string err;
	        bool ret = tinyobj::LoadObj(&attrib, &shapes, &materials, &err, filename, basepath, triangulate);

        if (!err.empty())
            fprintf(stderr, "\n%s\n", err.c_str());

        if (!ret)
            throw std::runtime_error("Erro ao carregar modelo.");

        printf("OK.\n");
    }
};

void BuildTrianglesAndAddToVirtualScene(ObjModel*); // Constrói representação de um ObjModel como malha de triângulos para renderização
void DrawPlane(GLint render_as_black_uniform);
void ComputeNormals(ObjModel* model); // Computa normais de um ObjModel, caso não existam.
GLuint BuildTriangles(); // Constrói triângulos para renderização
void LoadShadersFromFiles(); // Carrega os shaders de vértice e fragmento, criando um programa de GPU
void LoadTextureImage(const char* filename); // Função que carrega imagens de textura
void DrawVirtualObject(const char* object_name); // Desenha um objeto armazenado em g_VirtualScene
//void PrintObjModelInfo(ObjModel*); // Função para debugging


// Definimos uma estrutura que armazenará dados necessários para renderizar
// cada objeto da cena virtual.
struct SceneObject
{
    const char*  name;        // Nome do objeto
    void*        first_index; // Índice do primeiro vértice dentro do vetor indices[] definido em BuildTriangles()
    int          num_indices; // Número de índices do objeto dentro do vetor indices[] definido em BuildTriangles()
    GLenum       rendering_mode; // Modo de rasterização (GL_TRIANGLES, GL_TRIANGLE_STRIP, etc.)
};

struct Obstacle
{
    SceneObject objetc;
    bool moving;
};


struct SceneObject2
{
    std::string  name;        // Nome do objeto
    void*        first_index; // Índice do primeiro vértice dentro do vetor indices[] definido em BuildTrianglesAndAddToVirtualScene()
    int          num_indices; // Número de índices do objeto dentro do vetor indices[] definido em BuildTrianglesAndAddToVirtualScene()
    GLenum       rendering_mode; // Modo de rasterização (GL_TRIANGLES, GL_TRIANGLE_STRIP, etc.)
    GLuint       vertex_array_object_id; // ID do VAO onde estão armazenados os atributos do modelo
    glm::vec3    bbox_min; // Axis-Aligned Bounding Box do objeto
    glm::vec3    bbox_max;
};

// Abaixo definimos variáveis globais utilizadas em várias funções do código.

// A cena virtual é uma lista de objetos nomeados, guardados em um dicionário
// (map).  Veja dentro da função BuildTriangles() como que são incluídos
// objetos dentro da variável g_VirtualScene, e veja na função main() como
// estes são acessados.
std::map<const char*, SceneObject> g_VirtualScene;
std::map<std::string, SceneObject2> g_VirtualScene2;

// Pilha que guardará as matrizes de modelagem.
std::stack<glm::mat4>  g_MatrixStack;

// Razão de proporção da janela (largura/altura). Veja função FramebufferSizeCallback().
float g_ScreenRatio = 1.0f;

// Ângulos de Euler que controlam a rotação de um dos cubos da cena virtual
float g_AngleX = 0.0f;
float g_AngleY = 0.0f;
float g_AngleZ = 0.0f;

glm::mat4 projection;
glm::mat4 view;

// "g_LeftMouseButtonPressed = true" se o usuário está com o botão esquerdo do mouse
// pressionado no momento atual. Veja função MouseButtonCallback().
bool g_LeftMouseButtonPressed = false;
bool g_RightMouseButtonPressed = false; // Análogo para botão direito do mouse
bool g_MiddleMouseButtonPressed = false; // Análogo para botão do meio do mouse

/** VARIAVEIS DA CAMERA */

float camera_pitch = 0.0f;
float default_camera_yaw = PI / 2;
float camera_yaw = default_camera_yaw;
float g_CameraDistance = 3.5f; // Distância da câmera para a origem

glm::vec4 camera_position_c  = glm::vec4(-0.05f, 2.0f, -6.3f, 1.0f); // Ponto "c", centro da câmera
glm::vec4 camera_up_vector   = glm::vec4(0.0f,1.0f,0.0f,0.0f); // Vetor "up"

glm::vec4 camera_view_vector = glm::vec4(cos(camera_yaw) * cos(camera_pitch) , sin(camera_pitch), sin(camera_yaw) * cos(camera_pitch), 0.0f);
glm::vec4 w = -camera_view_vector;
glm::vec4 u = crossproduct(camera_up_vector, w);

bool free_cam_enabled = true;

int jumpStep = 0;
int movement = 0;
double timeWhenSpacePressed = 0;
double timeWhenLeftPressed = 0;
double timeWhenRightPressed = 0;
char legUp = 'n';
char prevLegUp = 'n';

int track = 1;

bool started = false;

// Variáveis que controlam rotação do antebraço direito
float g_RightForearmAngleZ = 0.0f;
float g_RightForearmAngleX = 0.0f;

// Variáveis que controlam rotação do antebraço esquerdo
float g_LeftForearmAngleZ = 0.0f;
float g_LeftForearmAngleX = 0.0f;

// Variáveis que controlam rotação do braço direito
float g_RightArmAngleX = 0.0f;
float g_RightArmAngleZ = 0.0f;

// Variáveis que controlam rotação do braço esquerdo
float g_LeftArmAngleX = 0.0f;
float g_LeftArmAngleZ = 0.0f;

// Variáveis que controlam translação do torso
float g_TorsoPositionX = 0.0f;
float g_TorsoPositionY = 0.0f;

// Variáveis que controlam rotação da perna esquerda
float g_LeftLegAngleX = 0.0f;
float g_LeftLegAngleZ = 0.0f;

// Variáveis que controlam rotação da perna direita
float g_RightLegAngleX = 0.0f;
float g_RightLegAngleZ = 0.0f;

// Variáveis que controlam rotação da canela esquerda
float g_LeftLowerLegAngleX = 0.0f;
float g_LeftLowerLegAngleZ = 0.0f;

// Variáveis que controlam rotação da canela direita
float g_RightLowerLegAngleX = 0.0f;
float g_RightLowerLegAngleZ = 0.0f;

void BuildCamera(GLint view_uniform, GLint projection_uniform);

void liftLeftLeg();
void liftRightLeg();
void fall();
void jump();
void smoothTransition();
void clearAngles();
void lowLeftLeg();
void lowRightLeg();
void moveLeftArmBackwards(int dir);
void moveRightArmBackwards(int dir);
void moveLeftArmForwards(int dir);
void moveRightArmForwards(int dir);


// Variável que controla o tipo de projeção utilizada: perspectiva ou ortográfica.
bool g_UsePerspectiveProjection = true;

// Variável que controla se o texto informativo será mostrado na tela.
bool g_ShowInfoText = true;
double timeDelta;

// Variáveis que definem um programa de GPU (shaders). Veja função LoadShadersFromFiles().
GLuint vertex_shader_id;
GLuint fragment_shader_id;
GLuint program_id = 0;
GLint model_uniform;
GLint view_uniform;
GLint projection_uniform;
GLint object_id_uniform;
GLint bbox_min_uniform;
GLint bbox_max_uniform;

// Número de texturas carregadas pela função LoadTextureImage()
GLuint g_NumLoadedTextures = 0;

//int main()
int main(int argc, char* argv[])
{
    // Inicializamos a biblioteca GLFW, utilizada para criar uma janela do
    // sistema operacional, onde poderemos renderizar com OpenGL.
    int success = glfwInit();
    if (!success)
    {
        fprintf(stderr, "ERROR: glfwInit() failed.\n");
        std::exit(EXIT_FAILURE);
    }

    // Definimos o callback para impressão de erros da GLFW no terminal
    glfwSetErrorCallback(ErrorCallback);

    // Pedimos para utilizar OpenGL versão 3.3 (ou superior)
    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 3);

    // Pedimos para utilizar o perfil "core", isto é, utilizaremos somente as
    // funções modernas de OpenGL.
    glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);

    // Criamos uma janela do sistema operacional, com 800 colunas e 480 linhas
    // de pixels, e com título "INF01047 ...".
    GLFWwindow* window;
    window = glfwCreateWindow(800, 800, "INF01047 - Julia Eidelwein (00274700) & Lucas Hagen (00274698)", NULL, NULL);
    if (!window)
    {
        glfwTerminate();
        fprintf(stderr, "ERROR: glfwCreateWindow() failed.\n");
        std::exit(EXIT_FAILURE);
    }

    // Definimos a função de callback que será chamada sempre que o usuário
    // pressionar alguma tecla do teclado ...
    glfwSetKeyCallback(window, KeyCallback);
    // ... ou clicar os botões do mouse ...
    glfwSetMouseButtonCallback(window, MouseButtonCallback);
    // ... ou movimentar o cursor do mouse em cima da janela ...
    glfwSetCursorPosCallback(window, CursorPosCallback);
    // ... ou rolar a "rodinha" do mouse.
    glfwSetScrollCallback(window, ScrollCallback);

    // Definimos a função de callback que será chamada sempre que a janela for
    // redimensionada, por consequência alterando o tamanho do "framebuffer"
    // (região de memória onde são armazenados os pixels da imagem).
    glfwSetFramebufferSizeCallback(window, FramebufferSizeCallback);
    glfwSetWindowSize(window, 800, 800); // Forçamos a chamada do callback acima, para definir g_ScreenRatio.

    // Indicamos que as chamadas OpenGL deverão renderizar nesta janela
    glfwMakeContextCurrent(window);

    // Carregamento de todas funções definidas por OpenGL 3.3, utilizando a
    // biblioteca GLAD.
    gladLoadGLLoader((GLADloadproc) glfwGetProcAddress);

    // Imprimimos no terminal informações sobre a GPU do sistema
    const GLubyte *vendor      = glGetString(GL_VENDOR);
    const GLubyte *renderer    = glGetString(GL_RENDERER);
    const GLubyte *glversion   = glGetString(GL_VERSION);
    const GLubyte *glslversion = glGetString(GL_SHADING_LANGUAGE_VERSION);

    printf("GPU: %s, %s, OpenGL %s, GLSL %s\n", vendor, renderer, glversion, glslversion);

    // Carregamos os shaders de vértices e de fragmentos que serão utilizados
    // para renderização. Veja slide 217 e 219 do documento no Moodle
    // "Aula_03_Rendering_Pipeline_Grafico.pdf".

    LoadShadersFromFiles();

    // Carregamos duas imagens para serem utilizadas como textura
    LoadTextureImage("../../data/tc-earth_daymap_surface.jpg");      // TextureImage0
    LoadTextureImage("../../data/tc-earth_nightmap_citylights.gif"); // TextureImage1
    LoadTextureImage("../../data/asphalt.png"); // TextureImage2
    //LoadTextureImage("../../data/asphalt.png"); // TextureImage2

    // Construímos a representação de objetos geométricos através de malhas de triângulos
    ObjModel spheremodel("../../data/sphere.obj");
    ComputeNormals(&spheremodel);
    BuildTrianglesAndAddToVirtualScene(&spheremodel);

    ObjModel bunnymodel("../../data/bunny.obj");
    ComputeNormals(&bunnymodel);
    BuildTrianglesAndAddToVirtualScene(&bunnymodel);

    ObjModel planemodel("../../data/plane.obj");
    ComputeNormals(&planemodel);
    BuildTrianglesAndAddToVirtualScene(&planemodel);

    ObjModel blockade("../../data/roadBlockade.obj");
    ComputeNormals(&blockade);
    BuildTrianglesAndAddToVirtualScene(&blockade);

    /*ObjModel floormodel("../../data/floor.obj");
    ComputeNormals(&floormodel);
    BuildTrianglesAndAddToVirtualScene(&floormodel);*/

    if ( argc > 1 )
    {
        ObjModel model(argv[1]);
        BuildTrianglesAndAddToVirtualScene(&model);
    }

    // Note que o caminho para os arquivos "shader_vertex.glsl" e
    // "shader_fragment.glsl" estão fixados, sendo que assumimos a existência
    // da seguinte estrutura no sistema de arquivos:
    //
    //    + FCG_Lab_0X/
    //    |
    //    +--+ bin/
    //    |  |
    //    |  +--+ Release/  (ou Debug/ ou Linux/)
    //    |     |
    //    |     o-- main.exe
    //    |
    //    +--+ src/
    //       |
    //       o-- shader_vertex.glsl
    //       |
    //       o-- shader_fragment.glsl
    //       |
    //       o-- ...
    //
    GLuint vertex_shader_id = LoadShader_Vertex("../../src/shader_vertex.glsl");
    GLuint fragment_shader_id = LoadShader_Fragment("../../src/shader_fragment.glsl");

    // Criamos um programa de GPU utilizando os shaders carregados acima
    GLuint program_id = CreateGpuProgram(vertex_shader_id, fragment_shader_id);

    // Construímos a representação de um triângulo
    GLuint vertex_array_object_id = BuildTriangles();

    // Inicializamos o código para renderização de texto.
    TextRendering_Init();

    // Buscamos o endereço das variáveis definidas dentro do Vertex Shader.
    // Utilizaremos estas variáveis para enviar dados para a placa de vídeo
    // (GPU)! Veja arquivo "shader_vertex.glsl".
    GLint model_uniform           = glGetUniformLocation(program_id, "model"); // Variável da matriz "model"
    GLint view_uniform            = glGetUniformLocation(program_id, "view"); // Variável da matriz "view" em shader_vertex.glsl
    GLint projection_uniform      = glGetUniformLocation(program_id, "projection"); // Variável da matriz "projection" em shader_vertex.glsl
    GLint render_as_black_uniform = glGetUniformLocation(program_id, "render_as_black"); // Variável booleana em shader_vertex.glsl
    // Habilitamos o Z-buffer. Veja slide 66 do documento "Aula_13_Clipping_and_Culling.pdf".
    glEnable(GL_DEPTH_TEST);

    // Habilitamos o Backface Culling. Veja slides 22 à 34 do documento "Aula_13_Clipping_and_Culling.pdf".
    glEnable(GL_CULL_FACE);
    glCullFace(GL_BACK);
    glFrontFace(GL_CCW);

    // Variáveis auxiliares utilizadas para chamada à função
    // TextRendering_ShowModelViewProjection(), armazenando matrizes 4x4.
    glm::mat4 the_projection;
    glm::mat4 the_model;
    glm::mat4 the_view;

    double prevTime = glfwGetTime();
    double currentTime;
    //double timeDelta;
    int obstacleDelta = 0;

    // Ficamos em loop, renderizando, até que o usuário feche a janela
    while (!glfwWindowShouldClose(window))
    {
        currentTime = glfwGetTime();
        timeDelta = currentTime - prevTime;
        prevTime = currentTime;
        // Aqui executamos as operações de renderização

        // Definimos a cor do "fundo" do framebuffer como branco.  Tal cor é
        // definida como coeficientes RGBA: Red, Green, Blue, Alpha; isto é:
        // Vermelho, Verde, Azul, Alpha (valor de transparência).
        // Conversaremos sobre sistemas de cores nas aulas de Modelos de Iluminação.
        //
        //           R     G     B     A
        glClearColor(1.0f, 1.0f, 1.0f, 1.0f);

        // "Pintamos" todos os pixels do framebuffer com a cor definida acima,
        // e também resetamos todos os pixels do Z-buffer (depth buffer).
        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

        // Pedimos para a GPU utilizar o programa de GPU criado acima (contendo
        // os shaders de vértice e fragmentos).
        glUseProgram(program_id);

        // "Ligamos" o VAO. Informamos que queremos utilizar os atributos de
        // vértices apontados pelo VAO criado pela função BuildTriangles(). Veja
        // comentários detalhados dentro da definição de BuildTriangles().
        glBindVertexArray(vertex_array_object_id);

        BuildCamera(view_uniform, projection_uniform);

        BuildCharacter(currentTime, model_uniform, render_as_black_uniform, program_id);

        int obstacleSpeed = started ? 200 : 0;///criar uma estrutura para cada obstaculo
        obstacleDelta = obstacleDelta + obstacleSpeed*timeDelta; ///criar uma estrutura para cada obstaclo
        glm::mat4 model = Matrix_Identity();
        model = model * Matrix_Translate(0.0f, 1.01f, 3.0f - obstacleDelta);
        glUniformMatrix4fv(model_uniform, 1, GL_FALSE, glm::value_ptr(model));
        DrawCube(render_as_black_uniform);

        model = Matrix_Identity();
        model = model * Matrix_Translate(0.0f, 1.01f, 0.0f - obstacleDelta);
        glUniformMatrix4fv(model_uniform, 1, GL_FALSE, glm::value_ptr(model));
        DrawCube(render_as_black_uniform);

        model = Matrix_Identity();
        model = model * Matrix_Translate(0.0f, 1.01f, -2.0f - obstacleDelta);
        glUniformMatrix4fv(model_uniform, 1, GL_FALSE, glm::value_ptr(model));
        DrawCube(render_as_black_uniform);

        model = Matrix_Identity();
        glUniformMatrix4fv(model_uniform, 1, GL_FALSE, glm::value_ptr(model));
        DrawPlane(render_as_black_uniform);

        // Neste ponto a matriz model recuperada é a matriz inicial (translação do torso)

        // Agora queremos desenhar os eixos XYZ de coordenadas GLOBAIS.
        // Para tanto, colocamos a matriz de modelagem igual à identidade.
        // Veja slide 147 do documento "Aula_08_Sistemas_de_Coordenadas.pdf".
        model = Matrix_Identity();

        // Enviamos a nova matriz "model" para a placa de vídeo (GPU). Veja o
        // arquivo "shader_vertex.glsl".
        glUniformMatrix4fv(model_uniform, 1, GL_FALSE, glm::value_ptr(model));
        glUniformMatrix4fv(view_uniform       , 1 , GL_FALSE , glm::value_ptr(view));
        glUniformMatrix4fv(projection_uniform , 1 , GL_FALSE , glm::value_ptr(projection));

        #define SPHERE 0
        #define BUNNY  1
        #define PLANE  2
        #define BLOCKADE 3

        // Desenhamos o modelo da esfera
        model = Matrix_Translate(-1.0f,0.0f,0.0f)
              * Matrix_Rotate_Z(0.6f)
              * Matrix_Rotate_X(0.2f)
              * Matrix_Rotate_Y(g_AngleY + (float)glfwGetTime() * 0.1f);
        glUniformMatrix4fv(model_uniform, 1 , GL_FALSE , glm::value_ptr(model));
        glUniform1i(object_id_uniform, SPHERE);
        DrawVirtualObject("sphere");

        // Desenhamos o modelo do coelho
        model = Matrix_Translate(1.0f,0.0f,0.0f)
              * Matrix_Rotate_X(g_AngleX + (float)glfwGetTime() * 0.1f);
        glUniformMatrix4fv(model_uniform, 1 , GL_FALSE , glm::value_ptr(model));
        glUniform1i(object_id_uniform, BUNNY);
        DrawVirtualObject("bunny");

        // Desenhamos o plano do chão
        //model = Matrix_Translate(0.0f,-1.1f,0.0f);
        /*model = Matrix_Identity();
        model = Matrix_Translate(-2.5f, 0.0f, 0.0f)
                *Matrix_Scale(2.0f, 1.0f, 36.0f);
        glUniformMatrix4fv(model_uniform, 1 , GL_FALSE , glm::value_ptr(model));
        glUniform1i(object_id_uniform, PLANE);
        DrawVirtualObject("plane");*/

        model = Matrix_Identity();
        model = Matrix_Translate(1.0f, 5.0f, 0.0f)*
               Matrix_Scale(4.0f, 4.0f, 3.0f);
        glUniformMatrix4fv(model_uniform, 1 , GL_FALSE , glm::value_ptr(model));
        glUniform1i(object_id_uniform, BLOCKADE);
        DrawVirtualObject("blockade");

        /*model = Matrix_Identity();
        glUniformMatrix4fv(model_uniform, 1 , GL_FALSE , glm::value_ptr(model));
        glUniform1i(object_id_uniform, FLOOR);
        DrawVirtualObject("floor");*/

        // Pedimos para OpenGL desenhar linhas com largura de 10 pixels.
        glLineWidth(10.0f);

        // Informamos para a placa de vídeo (GPU) que a variável booleana
        // "render_as_black" deve ser colocada como "false". Veja o arquivo
        // "shader_vertex.glsl".
        glUniform1i(render_as_black_uniform, false);

        // Pedimos para a GPU rasterizar os vértices dos eixos XYZ
        // apontados pelo VAO como linhas. Veja a definição de
        // g_VirtualScene["axes"] dentro da função BuildTriangles(), e veja
        // a documentação da função glDrawElements() em
        // http://docs.gl/gl3/glDrawElements.
        glDrawElements(
            g_VirtualScene["axes"].rendering_mode,
            g_VirtualScene["axes"].num_indices,
            GL_UNSIGNED_INT,
            (void*)g_VirtualScene["axes"].first_index
        );

        // "Desligamos" o VAO, evitando assim que operações posteriores venham a
        // alterar o mesmo. Isso evita bugs.
        glBindVertexArray(0);

        // Pegamos um vértice com coordenadas de modelo (0.5, 0.5, 0.5, 1) e o
        // passamos por todos os sistemas de coordenadas armazenados nas
        // matrizes the_model, the_view, e the_projection; e escrevemos na tela
        // as matrizes e pontos resultantes dessas transformações.
        //glm::vec4 p_model(0.5f, 0.5f, 0.5f, 1.0f);
        //TextRendering_ShowModelViewProjection(window, the_projection, the_view, the_model, p_model);

        // Imprimimos na tela os ângulos de Euler que controlam a rotação do
        // terceiro cubo.
        TextRendering_ShowEulerAngles(window);

        // Imprimimos na informação sobre a matriz de projeção sendo utilizada.
        TextRendering_ShowProjection(window);

        // Imprimimos na tela informação sobre o número de quadros renderizados
        // por segundo (frames per second).
        TextRendering_ShowFramesPerSecond(window);

        // O framebuffer onde OpenGL executa as operações de renderização não
        // é o mesmo que está sendo mostrado para o usuário, caso contrário
        // seria possível ver artefatos conhecidos como "screen tearing". A
        // chamada abaixo faz a troca dos buffers, mostrando para o usuário
        // tudo que foi renderizado pelas funções acima.
        // Veja o link: Veja o link: https://en.wikipedia.org/w/index.php?title=Multiple_buffering&oldid=793452829#Double_buffering_in_computer_graphics
        glfwSwapBuffers(window);

        // Verificamos com o sistema operacional se houve alguma interação do
        // usuário (teclado, mouse, ...). Caso positivo, as funções de callback
        // definidas anteriormente usando glfwSet*Callback() serão chamadas
        // pela biblioteca GLFW.
        glfwPollEvents();
    }

    // Finalizamos o uso dos recursos do sistema operacional
    glfwTerminate();

    // Fim do programa
    return 0;
}

void BuildCharacter(double currentTime, GLint model_uniform, GLint render_as_black_uniform, GLuint program_id) {
    // Buscamos o endereço das variáveis definidas dentro do Vertex Shader.
    // Utilizaremos estas variáveis para enviar dados para a placa de vídeo
    // (GPU)! Veja arquivo "shader_vertex.glsl".

    // Cada cópia do cubo possui uma matriz de modelagem independente,
    // já que cada cópia estará em uma posição (rotação, escala, ...)
    // diferente em relação ao espaço global (World Coordinates). Veja
    // slide 138 do documento "Aula_08_Sistemas_de_Coordenadas.pdf".
    //
    // Entretanto, neste laboratório as matrizes de modelagem dos cubos
    // serão construídas de maneira hierárquica, tal que operações em
    // alguns objetos influenciem outros objetos. Por exemplo: ao
    // transladar o torso, a cabeça deve se movimentar junto.
    // Veja slide 202 do documento "Aula_08_Sistemas_de_Coordenadas.pdf".
    //
    glm::mat4 model = Matrix_Identity(); // Transformação inicial = identidade.

    //double currentTime = glfwGetTime();
    //double timeDelta = currentTime - prevTime;

    // Translação inicial do torso
    //model = model * Matrix_Translate(g_TorsoPositionX - 1.0f, g_TorsoPositionY + 1.0f, 0.0f);
    model = model * Matrix_Translate(g_TorsoPositionX, g_TorsoPositionY + 1.83, -6.5f);
    //if(jumpStep > 0){
    switch(movement){
    case -1: //Falling
        if(!PlayerFloorColision(0.0, g_TorsoPositionY)){
            fall();
        } else {
            timeWhenSpacePressed = 0;
            clearAngles();
            if(started){
                movement = 2;
                //clearAngles();
                legUp = 'n';
            }
        }
        break;
    case 1://Jumping
        if(currentTime - timeWhenSpacePressed < 0.4){
            jump();
        } else {
            movement = 0;
        }
        break;
    case 2://Running
        switch(legUp){
        case 'n': //none
            if(prevLegUp == 'l' or prevLegUp == 'n'){
                liftRightLeg();
                moveLeftArmForwards(1);
                moveRightArmBackwards(1);
                if(g_RightLegAngleX <= -2){
                    legUp = 'r';
                }
            } else {
                liftLeftLeg();
                moveRightArmForwards(1);
                moveLeftArmBackwards(1);
                if(g_LeftLegAngleX <= -2){
                    legUp = 'l';
                }
            }
            break;
        case 'r': //right
            lowRightLeg();
            moveLeftArmForwards(-1);
            moveRightArmBackwards(-1);
            if(g_RightLegAngleX >= 0){
                clearAngles();
                prevLegUp = 'r';
                legUp = 'n';
            }
            break;
        case 'l': // left
            lowLeftLeg();
            moveRightArmForwards(-1);
            moveLeftArmBackwards(-1);
            if(g_LeftLegAngleX >= 0){
                //printf("%f\n", g_RightLegAngleX);
                clearAngles();
                prevLegUp = 'l';
                legUp = 'n';
            }
            break;
        }
        break;
        /*case 3: //Move Right
	        if(currentTime - timeWhenRightPressed < 0.47){
	            g_TorsoPositionX = g_TorsoPositionX - 2.5*timeDelta;
	        } else {
	            movement = 2;
	        }
	        break;
	    case 4: //Move Left
	        if(currentTime - timeWhenLeftPressed < 0.47){
	            g_TorsoPositionX = g_TorsoPositionX + 2.5*timeDelta;
	        } else {
	            movement = 2;
	        }
	        break;*/
    case 0://Nothing
    default:
        if(currentTime - timeWhenSpacePressed > 0.47){
            movement = -1;
        }
    }
    if(currentTime - timeWhenRightPressed < 0.4 && timeWhenRightPressed != 0){
        g_TorsoPositionX = g_TorsoPositionX - 4.3*timeDelta;
        camera_position_c.x = camera_position_c.x - 4.3*timeDelta;

    } else {
        if(currentTime - timeWhenLeftPressed < 0.4 && timeWhenLeftPressed != 0){
            g_TorsoPositionX = g_TorsoPositionX + 4.3*timeDelta;
            camera_position_c.x = camera_position_c.x + 4.3*timeDelta;
        } else {
            switch(track){
            case 0:
                camera_position_c.x = 1.95f;
                g_TorsoPositionX = 2.0f;
                break;
            case 1:
                camera_position_c.x = -0.05f;
                g_TorsoPositionX = 0.0f;
                break;
            case 2:
            default:
                camera_position_c.x = -2.05f;
                g_TorsoPositionX = -2.0f;
            }
        }
    }


    //}
    // Guardamos matriz model atual na pilha
    PushMatrix(model);
        // Atualizamos a matriz model (multiplicação à direita) para fazer um escalamento do torso
        model = model * Matrix_Scale(0.4f, 0.6f, 0.2f);
        // Enviamos a matriz "model" para a placa de vídeo (GPU). Veja o
        // arquivo "shader_vertex.glsl", onde esta é efetivamente
        // aplicada em todos os pontos.
        glUniformMatrix4fv(model_uniform, 1, GL_FALSE, glm::value_ptr(model));
        // Desenhamos um cubo. Esta renderização irá executar o Vertex
        // Shader definido no arquivo "shader_vertex.glsl", e o mesmo irá
        // utilizar as matrizes "model", "view" e "projection" definidas
        // acima e já enviadas para a placa de vídeo (GPU).
        DrawCube(render_as_black_uniform); // #### TORSO
    // Tiramos da pilha a matriz model guardada anteriormente
    PopMatrix(model);

    PushMatrix(model); // Guardamos matriz model atual na pilha
        model = model * Matrix_Translate(-0.32f, 0.0f, 0.0f); // Atualizamos matriz model (multiplicação à direita) com uma translação para o braço direito
        PushMatrix(model); // Guardamos matriz model atual na pilha
            model = model // Atualizamos matriz model (multiplicação à direita) com a rotação do braço direito
                  * Matrix_Rotate_Z(g_RightArmAngleZ)  // SEGUNDO rotação Z de Euler
                  * Matrix_Rotate_X(g_RightArmAngleX); // PRIMEIRO rotação X de Euler
            PushMatrix(model); // Guardamos matriz model atual na pilha
                model = model * Matrix_Scale(0.15f, 0.47f, 0.15f); // Atualizamos matriz model (multiplicação à direita) com um escalamento do braço direito
                glUniformMatrix4fv(model_uniform, 1, GL_FALSE, glm::value_ptr(model)); // Enviamos matriz model atual para a GPU
                DrawCube(render_as_black_uniform); // #### BRAÇO DIREITO // Desenhamos o braço direito
            PopMatrix(model); // Tiramos da pilha a matriz model guardada anteriormente
            PushMatrix(model); // Guardamos matriz model atual na pilha
                model = model * Matrix_Translate(0.0f, -0.5f, 0.0f); // Atualizamos matriz model (multiplicação à direita) com a translação do antebraço direito
                model = model // Atualizamos matriz model (multiplicação à direita) com a rotação do antebraço direito
                      * Matrix_Rotate_Z(g_RightForearmAngleZ)  // SEGUNDO rotação Z de Euler
                      * Matrix_Rotate_X(g_RightForearmAngleX); // PRIMEIRO rotação X de Euler
                PushMatrix(model); // Guardamos matriz model atual na pilha
                    model = model * Matrix_Scale(0.15f, 0.38f, 0.15f); // Atualizamos matriz model (multiplicação à direita) com um escalamento do antebraço direito
                    glUniformMatrix4fv(model_uniform, 1, GL_FALSE, glm::value_ptr(model)); // Enviamos matriz model atual para a GPU
                    DrawCube(render_as_black_uniform); // #### ANTEBRAÇO DIREITO // Desenhamos o antebraço direito
                    PushMatrix(model);
                        model = model * Matrix_Translate(0.0f, -1.1f, 0.0f);
                        PushMatrix(model);
                            model = model * Matrix_Scale(0.9f, 0.18f, 0.9f);
                            glUniformMatrix4fv(model_uniform, 1, GL_FALSE, glm::value_ptr(model));
                            DrawCube(render_as_black_uniform); // #### MÃO DIREITA
                    PopMatrix(model);
                PopMatrix(model);
            PopMatrix(model);
        PopMatrix(model);
    PopMatrix(model);

    PushMatrix(model); // Guardamos matriz model atual na pilha
        model = model * Matrix_Translate(0.635f, 0.0f, 0.0f); // Atualizamos matriz model (multiplicação à direita) com uma translação para o braço direito
        PushMatrix(model); // Guardamos matriz model atual na pilha
            model = model // Atualizamos matriz model (multiplicação à direita) com a rotação do braço direito
                  * Matrix_Rotate_Z(g_LeftArmAngleZ)  // SEGUNDO rotação Z de Euler
                  * Matrix_Rotate_X(g_LeftArmAngleX); // PRIMEIRO rotação X de Euler
            PushMatrix(model);
                model = model * Matrix_Scale(0.15f, 0.47f, 0.15f);
                glUniformMatrix4fv(model_uniform, 1, GL_FALSE, glm::value_ptr(model));
                DrawCube(render_as_black_uniform); // #### BRAÇO ESQUERDO
            PopMatrix(model);
            PushMatrix(model);
                model = model * Matrix_Translate(0.0f, -0.5f, 0.0f);
                model = model
                      * Matrix_Rotate_Z(g_LeftForearmAngleZ)
                      * Matrix_Rotate_X(g_LeftForearmAngleX);
                PushMatrix(model);
                    model = model * Matrix_Scale(0.15f, 0.38f, 0.15f);
                    glUniformMatrix4fv(model_uniform, 1, GL_FALSE, glm::value_ptr(model));
                    DrawCube(render_as_black_uniform); // #### ANTEBRAÇO ESQUERDO
                    PushMatrix(model);
                        model = model * Matrix_Translate(0.0f, -1.1f, 0.0f);
                        PushMatrix(model);
                            model = model * Matrix_Scale(0.9f, 0.18f, 0.9f);
                            glUniformMatrix4fv(model_uniform, 1, GL_FALSE, glm::value_ptr(model));
                            DrawCube(render_as_black_uniform); // #### MÃO ESQUERDA
                    PopMatrix(model);
                PopMatrix(model);
            PopMatrix(model);
        PopMatrix(model);
    PopMatrix(model);

    PushMatrix(model);
        model = model * Matrix_Translate(-0.315f, 0.05f, 0.0f);
        model = model * Matrix_Rotate_Y(3.141592) * Matrix_Rotate_X(3.141592);
        PushMatrix(model);
            PushMatrix(model);
                model = model * Matrix_Scale(0.25f, 0.25f, 0.25f);
                glUniformMatrix4fv(model_uniform, 1, GL_FALSE, glm::value_ptr(model));
                DrawCube(render_as_black_uniform); // #### CABEÇA
            PopMatrix(model);
        PopMatrix(model);
    PopMatrix(model);

    PushMatrix(model); // Guardamos matriz model atual na pilha
        model = model * Matrix_Translate(-0.415f, -0.66f, 0.0f); // Atualizamos matriz model (multiplicação à direita) com uma translação para o braço direito
        model = model
              * Matrix_Rotate_Z(g_RightLegAngleZ)  // SEGUNDO rotação Z de Euler
              * Matrix_Rotate_X(g_RightLegAngleX); // PRIMEIRO rotação X de Euler
        PushMatrix(model); // Guardamos matriz model atual na pilha
            PushMatrix(model);
                model = model * Matrix_Scale(0.18f, 0.53f, 0.18f);
                glUniformMatrix4fv(model_uniform, 1, GL_FALSE, glm::value_ptr(model));
                DrawCube(render_as_black_uniform); // #### PERNA DIREITA
            PopMatrix(model);
            PushMatrix(model);
                model = model * Matrix_Translate(0.0f, -0.57f, 0.0f);
                model = model
                        * Matrix_Rotate_Z(g_RightLowerLegAngleZ)  // SEGUNDO rotação Z de Euler
                        * Matrix_Rotate_X(g_RightLowerLegAngleX); // PRIMEIRO rotação X de Euler
                PushMatrix(model);
                    model = model * Matrix_Scale(0.17f, 0.5f, 0.17f);
                    glUniformMatrix4fv(model_uniform, 1, GL_FALSE, glm::value_ptr(model));
                    DrawCube(render_as_black_uniform); // #### CANELA DIREITA
                    PushMatrix(model);
                        model = model * Matrix_Translate(0.0f, -1.08f, 0.26f);
                        PushMatrix(model);
                            model = model * Matrix_Scale(0.78f, 0.095f, 1.2f);
                            glUniformMatrix4fv(model_uniform, 1, GL_FALSE, glm::value_ptr(model));
                            DrawCube(render_as_black_uniform); // #### PÉ DIREITO
                    PopMatrix(model);
                PopMatrix(model);
            PopMatrix(model);
        PopMatrix(model);
    PopMatrix(model);

    PushMatrix(model); // Guardamos matriz model atual na pilha
        model = model * Matrix_Translate(0.2f, 0.0f, 0.0f); // Atualizamos matriz model (multiplicação à direita) com uma translação para o braço direito
        model = model
              * Matrix_Rotate_Z(g_LeftLegAngleZ)  // SEGUNDO rotação Z de Euler
              * Matrix_Rotate_X(g_LeftLegAngleX); // PRIMEIRO rotação X de Euler
        PushMatrix(model); // Guardamos matriz model atual na pilha
            PushMatrix(model);
                model = model * Matrix_Scale(0.18f, 0.53f, 0.18f);
                glUniformMatrix4fv(model_uniform, 1, GL_FALSE, glm::value_ptr(model));
                DrawCube(render_as_black_uniform); // #### PERNA ESQUERDA
            PopMatrix(model);
            PushMatrix(model);
                model = model * Matrix_Translate(0.0f, -0.57f, 0.0f);
                model = model
                      * Matrix_Rotate_Z(g_LeftLowerLegAngleZ)  // SEGUNDO rotação Z de Euler
                      * Matrix_Rotate_X(g_LeftLowerLegAngleX); // PRIMEIRO rotação X de Euler
                PushMatrix(model);
                    model = model * Matrix_Scale(0.17f, 0.5f, 0.17f);
                    glUniformMatrix4fv(model_uniform, 1, GL_FALSE, glm::value_ptr(model));
                    DrawCube(render_as_black_uniform); // #### CANELA ESQUERDA
                    PushMatrix(model);
                        model = model * Matrix_Translate(0.0f, -1.08f, 0.26f);
                        PushMatrix(model);
                            model = model * Matrix_Scale(0.78f, 0.095f, 1.2f);
                            glUniformMatrix4fv(model_uniform, 1, GL_FALSE, glm::value_ptr(model));
                            DrawCube(render_as_black_uniform); // #### PÉ ESQUERDO
                    PopMatrix(model);
                PopMatrix(model);
            PopMatrix(model);
        PopMatrix(model);
    PopMatrix(model);
}

void BuildCamera(GLint view_uniform, GLint projection_uniform) {
    //glm::mat4 view;
    glm::vec4 look_at = glm::vec4(0.0f, 1.5f, 0.0f, 1.0f);

    if(free_cam_enabled) {
        view = Matrix_Camera_View(camera_position_c, camera_view_vector, camera_up_vector);
    } else {
        glm::vec4 new_cam_pos = camera_position_c + (-camera_view_vector * g_CameraDistance) / norm(camera_view_vector);
        //new_cam_pos.z = new_cam_pos.z - 8.0f;
        //new_cam_pos.y = new_cam_pos.y + 1.5f;
        view = Matrix_Camera_View(new_cam_pos,
                                  camera_view_vector, camera_up_vector);
    }

    //glm::mat4 projection;

    float nearplane = -0.1f;  // Posição do "near plane"
    float farplane  = -60.0f; // Posição do "far plane"

    if (g_UsePerspectiveProjection)
    {
        float field_of_view = 3.141592 / 3.0f;
        projection = Matrix_Perspective(field_of_view, g_ScreenRatio, nearplane, farplane);
    }
    else
    {
        float t = 1.5f*g_CameraDistance/2.5f;
        float b = -t;
        float r = t*g_ScreenRatio;
        float l = -r;
        projection = Matrix_Orthographic(l, r, b, t, nearplane, farplane);
    }

    // Enviamos as matrizes "view" e "projection" para a placa de vídeo
    // (GPU). Veja o arquivo "shader_vertex.glsl", onde estas são
    // efetivamente aplicadas em todos os pontos.
    glUniformMatrix4fv(view_uniform       , 1 , GL_FALSE , glm::value_ptr(view));
    glUniformMatrix4fv(projection_uniform , 1 , GL_FALSE , glm::value_ptr(projection));
}


// Função que carrega uma imagem para ser utilizada como textura
void LoadTextureImage(const char* filename)
{
    printf("Carregando imagem \"%s\"... ", filename);

    // Primeiro fazemos a leitura da imagem do disco
    stbi_set_flip_vertically_on_load(true);
    int width;
    int height;
    int channels;
    unsigned char *data = stbi_load(filename, &width, &height, &channels, 3);

    if ( data == NULL )
    {
        fprintf(stderr, "ERROR: Cannot open image file \"%s\".\n", filename);
        std::exit(EXIT_FAILURE);
    }

    printf("OK (%dx%d).\n", width, height);

    // Agora criamos objetos na GPU com OpenGL para armazenar a textura
    GLuint texture_id;
    GLuint sampler_id;
    glGenTextures(1, &texture_id);
    glGenSamplers(1, &sampler_id);

    // Veja slide 160 do documento "Aula_20_e_21_Mapeamento_de_Texturas.pdf"
    glSamplerParameteri(sampler_id, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glSamplerParameteri(sampler_id, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);

    // Parâmetros de amostragem da textura. Falaremos sobre eles em uma próxima aula.
    glSamplerParameteri(sampler_id, GL_TEXTURE_MIN_FILTER, GL_LINEAR_MIPMAP_LINEAR);
    glSamplerParameteri(sampler_id, GL_TEXTURE_MAG_FILTER, GL_LINEAR);

    // Agora enviamos a imagem lida do disco para a GPU
    glPixelStorei(GL_UNPACK_ALIGNMENT, 1);
    glPixelStorei(GL_UNPACK_ROW_LENGTH, 0);
    glPixelStorei(GL_UNPACK_SKIP_PIXELS, 0);
    glPixelStorei(GL_UNPACK_SKIP_ROWS, 0);

    GLuint textureunit = g_NumLoadedTextures;
    glActiveTexture(GL_TEXTURE0 + textureunit);
    glBindTexture(GL_TEXTURE_2D, texture_id);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_SRGB8, width, height, 0, GL_RGB, GL_UNSIGNED_BYTE, data);
    glGenerateMipmap(GL_TEXTURE_2D);
    glBindSampler(textureunit, sampler_id);

    stbi_image_free(data);

    g_NumLoadedTextures += 1;
}

// Função que desenha um objeto armazenado em g_VirtualScene. Veja definição
// dos objetos na função BuildTrianglesAndAddToVirtualScene().
void DrawVirtualObject(const char* object_name)
{
    // "Ligamos" o VAO. Informamos que queremos utilizar os atributos de
    // vértices apontados pelo VAO criado pela função BuildTrianglesAndAddToVirtualScene(). Veja
    // comentários detalhados dentro da definição de BuildTrianglesAndAddToVirtualScene().
    glBindVertexArray(g_VirtualScene2[object_name].vertex_array_object_id);

    // Setamos as variáveis "bbox_min" e "bbox_max" do fragment shader
    // com os parâmetros da axis-aligned bounding box (AABB) do modelo.
    glm::vec3 bbox_min = g_VirtualScene2[object_name].bbox_min;
    glm::vec3 bbox_max = g_VirtualScene2[object_name].bbox_max;
    glUniform4f(bbox_min_uniform, bbox_min.x, bbox_min.y, bbox_min.z, 1.0f);
    glUniform4f(bbox_max_uniform, bbox_max.x, bbox_max.y, bbox_max.z, 1.0f);

    // Pedimos para a GPU rasterizar os vértices dos eixos XYZ
    // apontados pelo VAO como linhas. Veja a definição de
    // g_VirtualScene[""] dentro da função BuildTrianglesAndAddToVirtualScene(), e veja
    // a documentação da função glDrawElements() em
    // http://docs.gl/gl3/glDrawElements.
    glDrawElements(
        g_VirtualScene2[object_name].rendering_mode,
        g_VirtualScene2[object_name].num_indices,
        GL_UNSIGNED_INT,
        (void*)g_VirtualScene2[object_name].first_index);

    // "Desligamos" o VAO, evitando assim que operações posteriores venham a
    // alterar o mesmo. Isso evita bugs.
    glBindVertexArray(0);
}

void LoadShadersFromFiles()
{

    vertex_shader_id = LoadShader_Vertex("../../src/shader_vertex.glsl");
    fragment_shader_id = LoadShader_Fragment("../../src/shader_fragment.glsl");

    // Deletamos o programa de GPU anterior, caso ele exista.
    if ( program_id != 0 )
        glDeleteProgram(program_id);

    // Criamos um programa de GPU utilizando os shaders carregados acima.
    program_id = CreateGpuProgram(vertex_shader_id, fragment_shader_id);

    // Buscamos o endereço das variáveis definidas dentro do Vertex Shader.
    // Utilizaremos estas variáveis para enviar dados para a placa de vídeo
    // (GPU)! Veja arquivo "shader_vertex.glsl" e "shader_fragment.glsl".
    model_uniform           = glGetUniformLocation(program_id, "model"); // Variável da matriz "model"
    view_uniform            = glGetUniformLocation(program_id, "view"); // Variável da matriz "view" em shader_vertex.glsl
    projection_uniform      = glGetUniformLocation(program_id, "projection"); // Variável da matriz "projection" em shader_vertex.glsl
    object_id_uniform       = glGetUniformLocation(program_id, "object_id"); // Variável "object_id" em shader_fragment.glsl
    bbox_min_uniform        = glGetUniformLocation(program_id, "bbox_min");
    bbox_max_uniform        = glGetUniformLocation(program_id, "bbox_max");

    // Variáveis em "shader_fragment.glsl" para acesso das imagens de textura
    glUseProgram(program_id);
    glUniform1i(glGetUniformLocation(program_id, "TextureImage0"), 0);
    glUniform1i(glGetUniformLocation(program_id, "TextureImage1"), 1);
    glUniform1i(glGetUniformLocation(program_id, "TextureImage2"), 2);
    glUseProgram(0);
}

// Função que computa as normais de um ObjModel, caso elas não tenham sido
// especificadas dentro do arquivo ".obj"
void ComputeNormals(ObjModel* model)
{
    if ( !model->attrib.normals.empty() )
        return;

    // Primeiro computamos as normais para todos os TRIÂNGULOS.
    // Segundo, computamos as normais dos VÉRTICES através do método proposto
    // por Gourad, onde a normal de cada vértice vai ser a média das normais de
    // todas as faces que compartilham este vértice.

    size_t num_vertices = model->attrib.vertices.size() / 3;

    std::vector<int> num_triangles_per_vertex(num_vertices, 0);
    std::vector<glm::vec4> vertex_normals(num_vertices, glm::vec4(0.0f,0.0f,0.0f,0.0f));

    for (size_t shape = 0; shape < model->shapes.size(); ++shape)
    {
        size_t num_triangles = model->shapes[shape].mesh.num_face_vertices.size();

        for (size_t triangle = 0; triangle < num_triangles; ++triangle)
        {
            assert(model->shapes[shape].mesh.num_face_vertices[triangle] == 3);

            glm::vec4  vertices[3];
            for (size_t vertex = 0; vertex < 3; ++vertex)
            {
                tinyobj::index_t idx = model->shapes[shape].mesh.indices[3*triangle + vertex];
                const float vx = model->attrib.vertices[3*idx.vertex_index + 0];
                const float vy = model->attrib.vertices[3*idx.vertex_index + 1];
                const float vz = model->attrib.vertices[3*idx.vertex_index + 2];
                vertices[vertex] = glm::vec4(vx,vy,vz,1.0);
            }

            const glm::vec4  a = vertices[0];
            const glm::vec4  b = vertices[1];
            const glm::vec4  c = vertices[2];

            const glm::vec4  n = crossproduct(b-a,c-a);

            for (size_t vertex = 0; vertex < 3; ++vertex)
            {
                tinyobj::index_t idx = model->shapes[shape].mesh.indices[3*triangle + vertex];
                num_triangles_per_vertex[idx.vertex_index] += 1;
                vertex_normals[idx.vertex_index] += n;
                model->shapes[shape].mesh.indices[3*triangle + vertex].normal_index = idx.vertex_index;
            }
        }
    }

    model->attrib.normals.resize( 3*num_vertices );

    for (size_t i = 0; i < vertex_normals.size(); ++i)
    {
        glm::vec4 n = vertex_normals[i] / (float)num_triangles_per_vertex[i];
        n /= norm(n);
        model->attrib.normals[3*i + 0] = n.x;
        model->attrib.normals[3*i + 1] = n.y;
        model->attrib.normals[3*i + 2] = n.z;
    }
}

// Constrói triângulos para futura renderização a partir de um ObjModel.
void BuildTrianglesAndAddToVirtualScene(ObjModel* model)
{
    GLuint vertex_array_object_id;
    glGenVertexArrays(1, &vertex_array_object_id);
    glBindVertexArray(vertex_array_object_id);

    std::vector<GLuint> indices;
    std::vector<float>  model_coefficients;
    std::vector<float>  normal_coefficients;
    std::vector<float>  texture_coefficients;

    for (size_t shape = 0; shape < model->shapes.size(); ++shape)
    {
        size_t first_index = indices.size();
        size_t num_triangles = model->shapes[shape].mesh.num_face_vertices.size();

        const float minval = std::numeric_limits<float>::min();
        const float maxval = std::numeric_limits<float>::max();

        glm::vec3 bbox_min = glm::vec3(maxval,maxval,maxval);
        glm::vec3 bbox_max = glm::vec3(minval,minval,minval);

        for (size_t triangle = 0; triangle < num_triangles; ++triangle)
        {
            assert(model->shapes[shape].mesh.num_face_vertices[triangle] == 3);

            for (size_t vertex = 0; vertex < 3; ++vertex)
            {
                tinyobj::index_t idx = model->shapes[shape].mesh.indices[3*triangle + vertex];

                indices.push_back(first_index + 3*triangle + vertex);

                const float vx = model->attrib.vertices[3*idx.vertex_index + 0];
                const float vy = model->attrib.vertices[3*idx.vertex_index + 1];
                const float vz = model->attrib.vertices[3*idx.vertex_index + 2];
                //printf("tri %d vert %d = (%.2f, %.2f, %.2f)\n", (int)triangle, (int)vertex, vx, vy, vz);
                model_coefficients.push_back( vx ); // X
                model_coefficients.push_back( vy ); // Y
                model_coefficients.push_back( vz ); // Z
                model_coefficients.push_back( 1.0f ); // W

                bbox_min.x = std::min(bbox_min.x, vx);
                bbox_min.y = std::min(bbox_min.y, vy);
                bbox_min.z = std::min(bbox_min.z, vz);
                bbox_max.x = std::max(bbox_max.x, vx);
                bbox_max.y = std::max(bbox_max.y, vy);
                bbox_max.z = std::max(bbox_max.z, vz);

                if ( idx.normal_index != -1 )
                {
                    const float nx = model->attrib.normals[3*idx.normal_index + 0];
                    const float ny = model->attrib.normals[3*idx.normal_index + 1];
                    const float nz = model->attrib.normals[3*idx.normal_index + 2];
                    normal_coefficients.push_back( nx ); // X
                    normal_coefficients.push_back( ny ); // Y
                    normal_coefficients.push_back( nz ); // Z
                    normal_coefficients.push_back( 0.0f ); // W
                }

                if ( idx.texcoord_index != -1 )
                {
                    const float u = model->attrib.texcoords[2*idx.texcoord_index + 0];
                    const float v = model->attrib.texcoords[2*idx.texcoord_index + 1];
                    texture_coefficients.push_back( u );
                    texture_coefficients.push_back( v );
                }
            }
        }

        size_t last_index = indices.size() - 1;

        SceneObject2 theobject;
        theobject.name           = model->shapes[shape].name;
        theobject.first_index    = (void*)first_index; // Primeiro índice
        theobject.num_indices    = last_index - first_index + 1; // Número de indices
        theobject.rendering_mode = GL_TRIANGLES;       // Índices correspondem ao tipo de rasterização GL_TRIANGLES.
        theobject.vertex_array_object_id = vertex_array_object_id;

        theobject.bbox_min = bbox_min;
        theobject.bbox_max = bbox_max;

        g_VirtualScene2[model->shapes[shape].name] = theobject;
    }

    GLuint VBO_model_coefficients_id;
    glGenBuffers(1, &VBO_model_coefficients_id);
    glBindBuffer(GL_ARRAY_BUFFER, VBO_model_coefficients_id);
    glBufferData(GL_ARRAY_BUFFER, model_coefficients.size() * sizeof(float), NULL, GL_STATIC_DRAW);
    glBufferSubData(GL_ARRAY_BUFFER, 0, model_coefficients.size() * sizeof(float), model_coefficients.data());
    GLuint location = 0; // "(location = 0)" em "shader_vertex.glsl"
    GLint  number_of_dimensions = 4; // vec4 em "shader_vertex.glsl"
    glVertexAttribPointer(location, number_of_dimensions, GL_FLOAT, GL_FALSE, 0, 0);
    glEnableVertexAttribArray(location);
    glBindBuffer(GL_ARRAY_BUFFER, 0);

    if ( !normal_coefficients.empty() )
    {
        GLuint VBO_normal_coefficients_id;
        glGenBuffers(1, &VBO_normal_coefficients_id);
        glBindBuffer(GL_ARRAY_BUFFER, VBO_normal_coefficients_id);
        glBufferData(GL_ARRAY_BUFFER, normal_coefficients.size() * sizeof(float), NULL, GL_STATIC_DRAW);
        glBufferSubData(GL_ARRAY_BUFFER, 0, normal_coefficients.size() * sizeof(float), normal_coefficients.data());
        location = 3; // "(location = 1)" em "shader_vertex.glsl"
        number_of_dimensions = 4; // vec4 em "shader_vertex.glsl"
        glVertexAttribPointer(location, number_of_dimensions, GL_FLOAT, GL_FALSE, 0, 0);
        glEnableVertexAttribArray(location);
        glBindBuffer(GL_ARRAY_BUFFER, 0);
    }

    if ( !texture_coefficients.empty() )
    {
        GLuint VBO_texture_coefficients_id;
        glGenBuffers(1, &VBO_texture_coefficients_id);
        glBindBuffer(GL_ARRAY_BUFFER, VBO_texture_coefficients_id);
        glBufferData(GL_ARRAY_BUFFER, texture_coefficients.size() * sizeof(float), NULL, GL_STATIC_DRAW);
        glBufferSubData(GL_ARRAY_BUFFER, 0, texture_coefficients.size() * sizeof(float), texture_coefficients.data());
        location = 2; // "(location = 1)" em "shader_vertex.glsl"
        number_of_dimensions = 2; // vec2 em "shader_vertex.glsl"
        glVertexAttribPointer(location, number_of_dimensions, GL_FLOAT, GL_FALSE, 0, 0);
        glEnableVertexAttribArray(location);
        glBindBuffer(GL_ARRAY_BUFFER, 0);
    }

    GLuint indices_id;
    glGenBuffers(1, &indices_id);
    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, indices_id);
    glBufferData(GL_ELEMENT_ARRAY_BUFFER, indices.size() * sizeof(GLuint), NULL, GL_STATIC_DRAW);
    glBufferSubData(GL_ELEMENT_ARRAY_BUFFER, 0, indices.size() * sizeof(GLuint), indices.data());
    // glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, 0); // XXX Errado!

    glBindVertexArray(0);
}


void liftLeftLeg(){
    g_RightLegAngleX = g_RightLegAngleX + 0.2*timeDelta;
    g_RightLowerLegAngleX = g_RightLowerLegAngleX + 1*timeDelta;
    g_LeftLegAngleX = g_LeftLegAngleX - 2.2*timeDelta;
    g_LeftLowerLegAngleX = g_LeftLowerLegAngleX + 2*timeDelta;
}
void liftRightLeg(){
    g_RightLegAngleX = g_RightLegAngleX - 2.2*timeDelta;
    g_RightLowerLegAngleX = g_RightLowerLegAngleX + 2*timeDelta;
    g_LeftLegAngleX = g_LeftLegAngleX + 2.5*timeDelta;
    g_LeftLowerLegAngleX = g_LeftLowerLegAngleX + 1*timeDelta;
}

void fall(){
    g_TorsoPositionY = g_TorsoPositionY - 2*timeDelta;
    camera_position_c.y = camera_position_c.y - 2*timeDelta;

    g_LeftForearmAngleZ = g_LeftForearmAngleZ - 1.7*timeDelta;
    g_LeftForearmAngleX = g_LeftForearmAngleX + 5*timeDelta;
    g_RightForearmAngleZ = g_RightForearmAngleZ + 1.7*timeDelta;
    g_RightForearmAngleX = g_RightForearmAngleX + 3*timeDelta;

    g_RightArmAngleX = g_RightArmAngleX + 3*timeDelta;
    g_RightArmAngleZ = g_RightArmAngleZ + 2*timeDelta;
    g_LeftArmAngleX = g_LeftArmAngleX + 2*timeDelta;
    g_LeftArmAngleZ = g_LeftArmAngleZ - 2*timeDelta;


    g_RightLegAngleX = g_RightLegAngleX + 2.2*timeDelta;
    g_RightLegAngleZ = g_RightLegAngleZ + 1*timeDelta;
    g_RightLowerLegAngleX = g_RightLowerLegAngleX - 3.4*timeDelta;
    g_RightLowerLegAngleZ = g_RightLowerLegAngleZ - 1*timeDelta;
    g_LeftLegAngleX = g_LeftLegAngleX + 1*timeDelta;
    g_LeftLowerLegAngleX = g_LeftLowerLegAngleX - 4.5*timeDelta;
    g_LeftLegAngleZ = g_LeftLegAngleZ - 1.2*timeDelta;
}
bool spacePressed;
float LeftArmAngleXDelta;
float RightArmAngleXDelta;
float LeftForearmAngleXDelta;
float RightForearmAngleXDelta;
float LeftLegAngleXDelta;
float RightLegAngleXDelta;
float LeftLowerLegAngleXDelta;
float RightLowerLegAngleXDelta;

float LeftArmAngleZDelta;
float RightArmAngleZDelta;
float LeftForearmAngleZDelta;
float RightForearmAngleZDelta;
float LeftLegAngleZDelta;
float RightLegAngleZDelta;
float LeftLowerLegAngleZDelta;
float RightLowerLegAngleZDelta;

void jump(){
    if(spacePressed){
        spacePressed = false;
        float desiredLeftArmAngleX = -0.806240;
        float desiredRightArmAngleX = -1.209359;
        float desiredLeftForearmAngleX = -2.015599;
        float desiredRightForearmAngleX = -1.209359;
        float desiredLeftLegAngleX = -0.403120;
        float desiredRightLegAngleX = -0.886863;
        float desiredLeftLowerLegAngleX = 1.814039;
        float desiredRightLowerLegAngleX = 1.370607;

        float desiredLeftArmAngleZ = 0.806240;
        float desiredRightArmAngleZ = -0.806240;
        float desiredLeftForearmAngleZ = 0.685304;
        float desiredRightForearmAngleZ = -0.685304;
        float desiredLeftLegAngleZ = 0.483744;
        float desiredRightLegAngleZ = -0.403120;
        float desiredLeftLowerLegAngleZ = 0.000000;
        float desiredRightLowerLegAngleZ = 0.403120;

        float totalIterationTime = 0.4;//timeDelta;

        LeftArmAngleXDelta = (desiredLeftArmAngleX - g_LeftArmAngleX);
        RightArmAngleXDelta = (desiredRightArmAngleX - g_RightArmAngleX);
        LeftForearmAngleXDelta = (desiredLeftForearmAngleX - g_LeftForearmAngleX);
        RightForearmAngleXDelta = (desiredRightForearmAngleX - g_RightForearmAngleX);
        LeftLegAngleXDelta = (desiredLeftLegAngleX - g_LeftLegAngleX);
        RightLegAngleXDelta = (desiredRightLegAngleX - g_RightLegAngleX);
        LeftLowerLegAngleXDelta = (desiredLeftLowerLegAngleX - g_LeftLowerLegAngleX);
        RightLowerLegAngleXDelta = (desiredRightLowerLegAngleX - g_RightLowerLegAngleX);

        LeftArmAngleZDelta = (desiredLeftArmAngleZ - g_LeftArmAngleZ);
        RightArmAngleZDelta = (desiredRightArmAngleZ - g_RightArmAngleZ);
        LeftForearmAngleZDelta = (desiredLeftForearmAngleZ - g_LeftForearmAngleZ);
        RightForearmAngleZDelta = (desiredRightForearmAngleZ - g_RightForearmAngleZ);
        LeftLegAngleZDelta = (desiredLeftLegAngleZ - g_LeftLegAngleZ);
        RightLegAngleZDelta = (desiredRightLegAngleZ - g_RightLegAngleZ);
        LeftLowerLegAngleZDelta = (desiredLeftLowerLegAngleZ - g_LeftLowerLegAngleZ);
        RightLowerLegAngleZDelta = (desiredRightLowerLegAngleZ - g_RightLowerLegAngleZ);

        LeftArmAngleXDelta = LeftArmAngleXDelta/totalIterationTime;
        RightArmAngleXDelta = RightArmAngleXDelta/totalIterationTime;
        LeftForearmAngleXDelta = LeftForearmAngleXDelta/totalIterationTime;
        RightForearmAngleXDelta = RightForearmAngleXDelta/totalIterationTime;
        LeftLegAngleXDelta = LeftLegAngleXDelta/totalIterationTime;
        RightLegAngleXDelta = RightLegAngleXDelta/totalIterationTime;
        LeftLowerLegAngleXDelta = LeftLowerLegAngleXDelta/totalIterationTime;
        RightLowerLegAngleXDelta = RightLowerLegAngleXDelta/totalIterationTime;

        LeftArmAngleZDelta = LeftArmAngleZDelta/totalIterationTime;
        RightArmAngleZDelta = RightArmAngleZDelta/totalIterationTime;
        LeftForearmAngleZDelta = LeftForearmAngleZDelta/totalIterationTime;
        RightForearmAngleZDelta = RightForearmAngleZDelta/totalIterationTime;
        LeftLegAngleZDelta = LeftLegAngleZDelta/totalIterationTime;
        RightLegAngleZDelta = RightLegAngleZDelta/totalIterationTime;
        LeftLowerLegAngleZDelta = LeftLowerLegAngleZDelta/totalIterationTime;
        RightLowerLegAngleZDelta = RightLowerLegAngleZDelta/totalIterationTime;

        /*printf("%f\n",LeftArmAngleXDelta);
        printf("%f\n",RightArmAngleXDelta);
        printf("%f\n",LeftForearmAngleXDelta);
        printf("%f\n",RightForearmAngleXDelta);
        printf("%f\n",LeftLegAngleXDelta);
        printf("%f\n",RightLegAngleXDelta);
        printf("%f\n",LeftLowerLegAngleXDelta);
        printf("%f\n",RightLowerLegAngleXDelta);

        printf("%f\n",LeftArmAngleZDelta);
        printf("%f\n",RightArmAngleZDelta);
        printf("%f\n",LeftForearmAngleZDelta);
        printf("%f\n",RightForearmAngleZDelta);
        printf("%f\n",LeftLegAngleZDelta);
        printf("%f\n",RightLegAngleZDelta);
        printf("%f\n",LeftLowerLegAngleZDelta);
        printf("%f\n",RightLowerLegAngleZDelta);*/
    }

    g_TorsoPositionY = g_TorsoPositionY + 2*timeDelta;
    camera_position_c.y = camera_position_c.y + 2*timeDelta;

    g_LeftForearmAngleZ = g_LeftForearmAngleZ + LeftForearmAngleZDelta*timeDelta;
    g_LeftForearmAngleX = g_LeftForearmAngleX + LeftForearmAngleXDelta*timeDelta;
    g_RightForearmAngleZ = g_RightForearmAngleZ + RightForearmAngleZDelta*timeDelta;
    g_RightForearmAngleX = g_RightForearmAngleX + RightForearmAngleXDelta*timeDelta;

    g_RightArmAngleX = g_RightArmAngleX + RightArmAngleXDelta*timeDelta;
    g_RightArmAngleZ = g_RightArmAngleZ + RightArmAngleZDelta*timeDelta;
    g_LeftArmAngleX = g_LeftArmAngleX + LeftArmAngleXDelta*timeDelta;
    g_LeftArmAngleZ = g_LeftArmAngleZ + LeftArmAngleZDelta*timeDelta;


    g_RightLegAngleX = g_RightLegAngleX + RightLegAngleXDelta*timeDelta;
    g_RightLegAngleZ = g_RightLegAngleZ + RightLegAngleZDelta*timeDelta;
    g_RightLowerLegAngleX = g_RightLowerLegAngleX + RightLowerLegAngleXDelta*timeDelta;
    g_RightLowerLegAngleZ = g_RightLowerLegAngleZ + RightLowerLegAngleZDelta*timeDelta;
    g_LeftLegAngleX = g_LeftLegAngleX + LeftLegAngleXDelta*timeDelta;
    g_LeftLowerLegAngleX = g_LeftLowerLegAngleX + LeftLowerLegAngleXDelta*timeDelta;
    g_LeftLegAngleZ = g_LeftLegAngleZ + LeftLegAngleZDelta*timeDelta;
}

void smoothTransition(){
    int smoothing = 2;
    g_LeftForearmAngleZ = g_LeftForearmAngleZ/smoothing;
    g_LeftForearmAngleX = g_LeftForearmAngleX/smoothing;
    g_RightForearmAngleZ = g_RightForearmAngleZ/smoothing;
    g_RightForearmAngleX = g_RightForearmAngleX/smoothing;

    g_RightArmAngleX = g_RightArmAngleX/smoothing;
    g_RightArmAngleZ = g_RightArmAngleZ/smoothing;
    g_LeftArmAngleX = g_LeftArmAngleX/smoothing;
    g_LeftArmAngleZ = g_LeftArmAngleZ/smoothing;


    g_RightLegAngleX = g_RightLegAngleX/smoothing;
    g_RightLegAngleZ = g_RightLegAngleZ/smoothing;
    g_RightLowerLegAngleX = g_RightLowerLegAngleX/smoothing;
    g_RightLowerLegAngleZ = g_RightLowerLegAngleZ/smoothing;
    g_LeftLegAngleX = g_LeftLegAngleX/smoothing;
    g_LeftLowerLegAngleX = g_LeftLowerLegAngleX/smoothing;
    g_LeftLegAngleZ = g_LeftLegAngleZ/smoothing;
}

void moveLeftArmForwards(int dir){
    g_LeftForearmAngleX = g_LeftForearmAngleX - dir*2.3*timeDelta;

    g_LeftArmAngleX = g_LeftArmAngleX - dir*1*timeDelta;
    g_LeftArmAngleZ = g_LeftArmAngleZ + dir*0.2*timeDelta;
}

void moveRightArmForwards(int dir){
    g_RightForearmAngleX = g_RightForearmAngleX - dir*2.3*timeDelta;

    g_RightArmAngleX = g_RightArmAngleX - dir*1*timeDelta;
    g_RightArmAngleZ = g_RightArmAngleZ - dir*0.2*timeDelta;
}

void moveRightArmBackwards(int dir){
    g_RightForearmAngleX = g_RightForearmAngleX - dir*2*timeDelta;

    g_RightArmAngleX = g_RightArmAngleX + dir*1*timeDelta;
    g_RightArmAngleZ = g_RightArmAngleZ - dir*0.2*timeDelta;
}

void moveLeftArmBackwards(int dir){
    g_LeftForearmAngleX = g_LeftForearmAngleX - dir*2*timeDelta;

    g_LeftArmAngleX = g_LeftArmAngleX + dir*1*timeDelta;
    g_LeftArmAngleZ = g_LeftArmAngleZ + dir*0.2*timeDelta;
}

void clearAngles(){
    g_LeftForearmAngleZ = 0.0f;
    g_LeftForearmAngleX = 0.0f;
    g_RightForearmAngleZ = 0.0f;
    g_RightForearmAngleX = 0.0f;

    g_RightArmAngleX = 0.0f;
    g_RightArmAngleZ = 0.0f;
    g_LeftArmAngleX = 0.0f;
    g_LeftArmAngleZ = 0.0f;


    g_RightLegAngleX = 0.0f;
    g_RightLegAngleZ = 0.0f;
    g_RightLowerLegAngleX = 0.0f;
    g_RightLowerLegAngleZ = 0.0f;
    g_LeftLegAngleX = 0.0f;
    g_LeftLowerLegAngleX = 0.0f;
    g_LeftLegAngleZ = 0.0f;
}

void lowLeftLeg(){
    g_RightLegAngleX = g_RightLegAngleX - 0.8*timeDelta;
    g_LeftLegAngleX = g_LeftLegAngleX + 4*timeDelta;
    g_LeftLowerLegAngleX = g_LeftLowerLegAngleX - 4*timeDelta;
}
void lowRightLeg(){
    g_RightLegAngleX = g_RightLegAngleX + 2.2*timeDelta;
    g_RightLowerLegAngleX = g_RightLowerLegAngleX - 2*timeDelta;
    g_LeftLegAngleX = g_LeftLegAngleX - 3.1*timeDelta;
}

// Função que pega a matriz M e guarda a mesma no topo da pilha
void PushMatrix(glm::mat4 M)
{
    g_MatrixStack.push(M);
}

// Função que remove a matriz atualmente no topo da pilha e armazena a mesma na variável M
void PopMatrix(glm::mat4& M)
{
    if ( g_MatrixStack.empty() )
    {
        M = Matrix_Identity();
    }
    else
    {
        M = g_MatrixStack.top();
        g_MatrixStack.pop();
    }
}
void DrawPlane(GLint render_as_black_uniform)
{
    glUniform1i(render_as_black_uniform, false);
    glDrawElements(
        g_VirtualScene["floor_plane"].rendering_mode, // Veja slide 175 do documento "Aula_04_Modelagem_Geometrica_3D.pdf".
        g_VirtualScene["floor_plane"].num_indices,    //
        GL_UNSIGNED_INT,
        (void*)g_VirtualScene["floor_plane"].first_index
    );

    glUniform1i(render_as_black_uniform, true);
}


// Função que desenha um cubo com arestas em preto, definido dentro da função BuildTriangles().
void DrawCube(GLint render_as_black_uniform)
{
    // Informamos para a placa de vídeo (GPU) que a variável booleana
    // "render_as_black" deve ser colocada como "false". Veja o arquivo
    // "shader_vertex.glsl".
    glUniform1i(render_as_black_uniform, false);

    // Pedimos para a GPU rasterizar os vértices do cubo apontados pelo
    // VAO como triângulos, formando as faces do cubo. Esta
    // renderização irá executar o Vertex Shader definido no arquivo
    // "shader_vertex.glsl", e o mesmo irá utilizar as matrizes
    // "model", "view" e "projection" definidas acima e já enviadas
    // para a placa de vídeo (GPU).
    //
    // Veja a definição de g_VirtualScene["cube_faces"] dentro da
    // função BuildTriangles(), e veja a documentação da função
    // glDrawElements() em http://docs.gl/gl3/glDrawElements.
    glDrawElements(
        g_VirtualScene["cube_faces"].rendering_mode, // Veja slide 175 do documento "Aula_04_Modelagem_Geometrica_3D.pdf".
        g_VirtualScene["cube_faces"].num_indices,    //
        GL_UNSIGNED_INT,
        (void*)g_VirtualScene["cube_faces"].first_index
    );

    // Pedimos para OpenGL desenhar linhas com largura de 4 pixels.
    glLineWidth(4.0f);

    // Pedimos para a GPU rasterizar os vértices dos eixos XYZ
    // apontados pelo VAO como linhas. Veja a definição de
    // g_VirtualScene["axes"] dentro da função BuildTriangles(), e veja
    // a documentação da função glDrawElements() em
    // http://docs.gl/gl3/glDrawElements.
    //
    // Importante: estes eixos serão desenhamos com a matriz "model"
    // definida acima, e portanto sofrerão as mesmas transformações
    // geométricas que o cubo. Isto é, estes eixos estarão
    // representando o sistema de coordenadas do modelo (e não o global)!
    glDrawElements(
        g_VirtualScene["axes"].rendering_mode,
        g_VirtualScene["axes"].num_indices,
        GL_UNSIGNED_INT,
        (void*)g_VirtualScene["axes"].first_index
    );

    // Informamos para a placa de vídeo (GPU) que a variável booleana
    // "render_as_black" deve ser colocada como "true". Veja o arquivo
    // "shader_vertex.glsl".
    glUniform1i(render_as_black_uniform, true);

    // Pedimos para a GPU rasterizar os vértices do cubo apontados pelo
    // VAO como linhas, formando as arestas pretas do cubo. Veja a
    // definição de g_VirtualScene["cube_edges"] dentro da função
    // BuildTriangles(), e veja a documentação da função
    // glDrawElements() em http://docs.gl/gl3/glDrawElements.
    glDrawElements(
        g_VirtualScene["cube_edges"].rendering_mode,
        g_VirtualScene["cube_edges"].num_indices,
        GL_UNSIGNED_INT,
        (void*)g_VirtualScene["cube_edges"].first_index
    );
}

// Constrói triângulos para futura renderização
GLuint BuildTriangles()
{
    // Primeiro, definimos os atributos de cada vértice.

    // A posição de cada vértice é definida por coeficientes em um sistema de
    // coordenadas local de cada modelo geométrico. Note o uso de coordenadas
    // homogêneas.  Veja as seguintes referências:
    //
    //  - slide 32  do documento "Aula_08_Sistemas_de_Coordenadas.pdf";
    //  - slide 147 do documento "Aula_08_Sistemas_de_Coordenadas.pdf";
    //  - slide 144 do documento "Aula_09_Projecoes.pdf".
    //
    // Este vetor "model_coefficients" define a GEOMETRIA (veja slide 112 do
    // documento "Aula_04_Modelagem_Geometrica_3D.pdf").
    //
    GLfloat model_coefficients[] = {
    // Vértices de um cubo
    //    X      Y     Z     W
        -0.5f,  0.0f,  0.5f, 1.0f, // posição do vértice 0
        -0.5f, -1.0f,  0.5f, 1.0f, // posição do vértice 1
         0.5f, -1.0f,  0.5f, 1.0f, // posição do vértice 2
         0.5f,  0.0f,  0.5f, 1.0f, // posição do vértice 3
        -0.5f,  0.0f, -0.5f, 1.0f, // posição do vértice 4
        -0.5f, -1.0f, -0.5f, 1.0f, // posição do vértice 5
         0.5f, -1.0f, -0.5f, 1.0f, // posição do vértice 6
         0.5f,  0.0f, -0.5f, 1.0f, // posição do vértice 7
    // Vértices para desenhar o plano
    //    X      Y     Z     W
         4.0f,  0.0f,  60.0f, 1.0f,
         4.0f,  0.0f,  -12.0f, 1.0f,
        -4.0f,  0.0f,  60.0f, 1.0f,
        -4.0f,  0.0f,  -12.0f, 1.0f

    // Vértices para desenhar o eixo X
    //    X      Y     Z     W
    //     0.0f,  0.0f,  0.0f, 1.0f, // posição do vértice 8
     //    1.0f,  0.0f,  0.0f, 1.0f, // posição do vértice 9
    // Vértices para desenhar o eixo Y
    //    X      Y     Z     W
     //    0.0f,  0.0f,  0.0f, 1.0f, // posição do vértice 10
     //    0.0f,  1.0f,  0.0f, 1.0f, // posição do vértice 11
    // Vértices para desenhar o eixo Z
    //    X      Y     Z     W
    //     0.0f,  0.0f,  0.0f, 1.0f, // posição do vértice 12
    //     0.0f,  0.0f,  1.0f, 1.0f, // posição do vértice 13
    };

    // Criamos o identificador (ID) de um Vertex Buffer Object (VBO).  Um VBO é
    // um buffer de memória que irá conter os valores de um certo atributo de
    // um conjunto de vértices; por exemplo: posição, cor, normais, coordenadas
    // de textura.  Neste exemplo utilizaremos vários VBOs, um para cada tipo de atributo.
    // Agora criamos um VBO para armazenarmos um atributo: posição.
    GLuint VBO_model_coefficients_id;
    glGenBuffers(1, &VBO_model_coefficients_id);

    // Criamos o identificador (ID) de um Vertex Array Object (VAO).  Um VAO
    // contém a definição de vários atributos de um certo conjunto de vértices;
    // isto é, um VAO irá conter ponteiros para vários VBOs.
    GLuint vertex_array_object_id;
    glGenVertexArrays(1, &vertex_array_object_id);

    // "Ligamos" o VAO ("bind"). Informamos que iremos atualizar o VAO cujo ID
    // está contido na variável "vertex_array_object_id".
    glBindVertexArray(vertex_array_object_id);

    // "Ligamos" o VBO ("bind"). Informamos que o VBO cujo ID está contido na
    // variável VBO_model_coefficients_id será modificado a seguir. A
    // constante "GL_ARRAY_BUFFER" informa que esse buffer é de fato um VBO, e
    // irá conter atributos de vértices.
    glBindBuffer(GL_ARRAY_BUFFER, VBO_model_coefficients_id);

    // Alocamos memória para o VBO "ligado" acima. Como queremos armazenar
    // nesse VBO todos os valores contidos no array "model_coefficients", pedimos
    // para alocar um número de bytes exatamente igual ao tamanho ("size")
    // desse array. A constante "GL_STATIC_DRAW" dá uma dica para o driver da
    // GPU sobre como utilizaremos os dados do VBO. Neste caso, estamos dizendo
    // que não pretendemos alterar tais dados (são estáticos: "STATIC"), e
    // também dizemos que tais dados serão utilizados para renderizar ou
    // desenhar ("DRAW").  Pense que:
    //
    //            glBufferData()  ==  malloc() do C  ==  new do C++.
    //
    glBufferData(GL_ARRAY_BUFFER, sizeof(model_coefficients), NULL, GL_DYNAMIC_DRAW);

    // Finalmente, copiamos os valores do array model_coefficients para dentro do
    // VBO "ligado" acima.  Pense que:
    //
    //            glBufferSubData()  ==  memcpy() do C.
    //
    glBufferSubData(GL_ARRAY_BUFFER, 0, sizeof(model_coefficients), model_coefficients);

    // Precisamos então informar um índice de "local" ("location"), o qual será
    // utilizado no shader "shader_vertex.glsl" para acessar os valores
    // armazenados no VBO "ligado" acima. Também, informamos a dimensão (número de
    // coeficientes) destes atributos. Como em nosso caso são pontos em coordenadas
    // homogêneas, temos quatro coeficientes por vértice (X,Y,Z,W). Isso define
    // um tipo de dado chamado de "vec4" em "shader_vertex.glsl": um vetor com
    // quatro coeficientes. Finalmente, informamos que os dados estão em ponto
    // flutuante com 32 bits (GL_FLOAT).
    // Esta função também informa que o VBO "ligado" acima em glBindBuffer()
    // está dentro do VAO "ligado" acima por glBindVertexArray().
    // Veja https://www.khronos.org/opengl/wiki/Vertex_Specification#Vertex_Buffer_Object
    GLuint location = 0; // "(location = 0)" em "shader_vertex.glsl"
    GLint  number_of_dimensions = 4; // vec4 em "shader_vertex.glsl"
    glVertexAttribPointer(location, number_of_dimensions, GL_FLOAT, GL_FALSE, 0, 0);

    // "Ativamos" os atributos. Informamos que os atributos com índice de local
    // definido acima, na variável "location", deve ser utilizado durante o
    // rendering.
    glEnableVertexAttribArray(location);

    // "Desligamos" o VBO, evitando assim que operações posteriores venham a
    // alterar o mesmo. Isso evita bugs.
    glBindBuffer(GL_ARRAY_BUFFER, 0);

    // Agora repetimos todos os passos acima para atribuir um novo atributo a
    // cada vértice: uma cor (veja slide 113 do documento "Aula_04_Modelagem_Geometrica_3D.pdf").
    // Tal cor é definida como coeficientes RGBA: Red, Green, Blue, Alpha;
    // isto é: Vermelho, Verde, Azul, Alpha (valor de transparência).
    // Conversaremos sobre sistemas de cores nas aulas de Modelos de Iluminação.
    GLfloat color_coefficients[] = {
    // Cores dos vértices do cubo
    //  R     G     B     A
        1.0f, 0.5f, 0.0f, 1.0f, // cor do vértice 0
        1.0f, 0.5f, 0.0f, 1.0f, // cor do vértice 1
        0.0f, 0.5f, 1.0f, 1.0f, // cor do vértice 2
        0.0f, 0.5f, 1.0f, 1.0f, // cor do vértice 3
        1.0f, 0.5f, 0.0f, 1.0f, // cor do vértice 4
        1.0f, 0.5f, 0.0f, 1.0f, // cor do vértice 5
        0.0f, 0.5f, 1.0f, 1.0f, // cor do vértice 6
        0.0f, 0.5f, 1.0f, 1.0f, // cor do vértice 7
    //Cores para desenhar o plano
        0.5f, 0.5f, 0.5f, 1.0f,
        0.5f, 0.5f, 0.5f, 1.0f,
        0.5f, 0.5f, 0.5f, 1.0f,
        0.5f, 0.5f, 0.5f, 1.0f,
    // Cores para desenhar o eixo X
        //1.0f, 0.0f, 0.0f, 1.0f, // cor do vértice 8
        //1.0f, 0.0f, 0.0f, 1.0f, // cor do vértice 9
    // Cores para desenhar o eixo Y
        //0.0f, 1.0f, 0.0f, 1.0f, // cor do vértice 10
        //0.0f, 1.0f, 0.0f, 1.0f, // cor do vértice 11
    // Cores para desenhar o eixo Z
        //0.0f, 0.0f, 1.0f, 1.0f, // cor do vértice 12
        //0.0f, 0.0f, 1.0f, 1.0f, // cor do vértice 13
    };
    GLuint VBO_color_coefficients_id;
    glGenBuffers(1, &VBO_color_coefficients_id);
    glBindBuffer(GL_ARRAY_BUFFER, VBO_color_coefficients_id);
    glBufferData(GL_ARRAY_BUFFER, sizeof(color_coefficients), NULL, GL_STATIC_DRAW);
    glBufferSubData(GL_ARRAY_BUFFER, 0, sizeof(color_coefficients), color_coefficients);
    location = 1; // "(location = 1)" em "shader_vertex.glsl"
    number_of_dimensions = 4; // vec4 em "shader_vertex.glsl"
    glVertexAttribPointer(location, number_of_dimensions, GL_FLOAT, GL_FALSE, 0, 0);
    glEnableVertexAttribArray(location);
    glBindBuffer(GL_ARRAY_BUFFER, 0);

    // Vamos então definir polígonos utilizando os vértices do array
    // model_coefficients.
    //
    // Para referência sobre os modos de renderização, veja slide 160 do
    // documento "Aula_04_Modelagem_Geometrica_3D.pdf".
    //
    // Este vetor "indices" define a TOPOLOGIA (veja slide 112 do documento
    // "Aula_04_Modelagem_Geometrica_3D.pdf").
    //
    GLuint indices[] = {
    // Definimos os índices dos vértices que definem as FACES de um cubo
    // através de 12 triângulos que serão desenhados com o modo de renderização
    // GL_TRIANGLES.
        0, 1, 2, // triângulo 1
        7, 6, 5, // triângulo 2
        3, 2, 6, // triângulo 3
        4, 0, 3, // triângulo 4
        4, 5, 1, // triângulo 5
        1, 5, 6, // triângulo 6
        0, 2, 3, // triângulo 7
        7, 5, 4, // triângulo 8
        3, 6, 7, // triângulo 9
        4, 3, 7, // triângulo 10
        4, 1, 0, // triângulo 11
        1, 6, 2, // triângulo 12
    // Definimos os índices dos vértices que definem as ARESTAS de um cubo
    // através de 12 linhas que serão desenhadas com o modo de renderização
    // GL_LINES.
        0, 1, // linha 1
        1, 2, // linha 2
        2, 3, // linha 3
        3, 0, // linha 4
        0, 4, // linha 5
        4, 7, // linha 6
        7, 6, // linha 7
        6, 2, // linha 8
        6, 5, // linha 9
        5, 4, // linha 10
        5, 1, // linha 11
        7, 3, // linha 12
    // Índices dos vértices do plano
        10, 8, 9,
        9, 11, 10
    // Definimos os índices dos vértices que definem as linhas dos eixos X, Y,
    // Z, que serão desenhados com o modo GL_LINES.
     //   8 , 9 , // linha 1
      //  10, 11, // linha 2
      //  12, 13  // linha 3
    };

    // Criamos um primeiro objeto virtual (SceneObject) que se refere às faces
    // coloridas do cubo.
    SceneObject cube_faces;
    cube_faces.name           = "Cubo (faces coloridas)";
    cube_faces.first_index    = (void*)0; // Primeiro índice está em indices[0]
    cube_faces.num_indices    = 36;       // Último índice está em indices[35]; total de 36 índices.
    cube_faces.rendering_mode = GL_TRIANGLES; // Índices correspondem ao tipo de rasterização GL_TRIANGLES.

    // Adicionamos o objeto criado acima na nossa cena virtual (g_VirtualScene).
    g_VirtualScene["cube_faces"] = cube_faces;

    // Criamos um segundo objeto virtual (SceneObject) que se refere às arestas
    // pretas do cubo.
    SceneObject cube_edges;
    cube_edges.name           = "Cubo (arestas pretas)";
    cube_edges.first_index    = (void*)(36*sizeof(GLuint)); // Primeiro índice está em indices[36]
    cube_edges.num_indices    = 24; // Último índice está em indices[59]; total de 24 índices.
    cube_edges.rendering_mode = GL_LINES; // Índices correspondem ao tipo de rasterização GL_LINES.

    // Adicionamos o objeto criado acima na nossa cena virtual (g_VirtualScene).
    g_VirtualScene["cube_edges"] = cube_edges;

    // Criamos um terceiro objeto virtual (SceneObject) que se refere aos eixos XYZ.
    //SceneObject axes;
    //axes.name           = "Eixos XYZ";
    //axes.first_index    = (void*)(60*sizeof(GLuint)); // Primeiro índice está em indices[60]
    //axes.num_indices    = 6; // Último índice está em indices[65]; total de 6 índices.
    //axes.rendering_mode = GL_LINES; // Índices correspondem ao tipo de rasterização GL_LINES.
    //g_VirtualScene["axes"] = axes;

    SceneObject floor_plane;
    floor_plane.name        = "Floor";
    floor_plane.first_index = (void*)(60*sizeof(GLuint));
    floor_plane.num_indices = 6;
    floor_plane.rendering_mode = GL_TRIANGLES;
    g_VirtualScene["floor_plane"] = floor_plane;

    // Criamos um buffer OpenGL para armazenar os índices acima
    GLuint indices_id;
    glGenBuffers(1, &indices_id);

    // "Ligamos" o buffer. Note que o tipo agora é GL_ELEMENT_ARRAY_BUFFER.
    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, indices_id);

    // Alocamos memória para o buffer.
    glBufferData(GL_ELEMENT_ARRAY_BUFFER, sizeof(indices), NULL, GL_DYNAMIC_DRAW);

    // Copiamos os valores do array indices[] para dentro do buffer.
    glBufferSubData(GL_ELEMENT_ARRAY_BUFFER, 0, sizeof(indices), indices);

    // NÃO faça a chamada abaixo! Diferente de um VBO (GL_ARRAY_BUFFER), um
    // array de índices (GL_ELEMENT_ARRAY_BUFFER) não pode ser "desligado",
    // caso contrário o VAO irá perder a informação sobre os índices.
    //
    // glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, 0); // XXX Errado!
    //

    // "Desligamos" o VAO, evitando assim que operações posteriores venham a
    // alterar o mesmo. Isso evita bugs.
    glBindVertexArray(0);

    // Retornamos o ID do VAO. Isso é tudo que será necessário para renderizar
    // os triângulos definidos acima. Veja a chamada glDrawElements() em main().
    return vertex_array_object_id;
}

// Carrega um Vertex Shader de um arquivo GLSL. Veja definição de LoadShader() abaixo.
GLuint LoadShader_Vertex(const char* filename)
{
    // Criamos um identificador (ID) para este shader, informando que o mesmo
    // será aplicado nos vértices.
    GLuint vertex_shader_id = glCreateShader(GL_VERTEX_SHADER);

    // Carregamos e compilamos o shader
    LoadShader(filename, vertex_shader_id);

    // Retorna o ID gerado acima
    return vertex_shader_id;
}

// Carrega um Fragment Shader de um arquivo GLSL . Veja definição de LoadShader() abaixo.
GLuint LoadShader_Fragment(const char* filename)
{
    // Criamos um identificador (ID) para este shader, informando que o mesmo
    // será aplicado nos fragmentos.
    GLuint fragment_shader_id = glCreateShader(GL_FRAGMENT_SHADER);

    // Carregamos e compilamos o shader
    LoadShader(filename, fragment_shader_id);

    // Retorna o ID gerado acima
    return fragment_shader_id;
}

// Função auxilar, utilizada pelas duas funções acima. Carrega código de GPU de
// um arquivo GLSL e faz sua compilação.
void LoadShader(const char* filename, GLuint shader_id)
{
    // Lemos o arquivo de texto indicado pela variável "filename"
    // e colocamos seu conteúdo em memória, apontado pela variável
    // "shader_string".
    std::ifstream file;
    try {
        file.exceptions(std::ifstream::failbit);
        file.open(filename);
    } catch ( std::exception& e ) {
        fprintf(stderr, "ERROR: Cannot open file \"%s\".\n", filename);
        std::exit(EXIT_FAILURE);
    }
    std::stringstream shader;
    shader << file.rdbuf();
    std::string str = shader.str();
    const GLchar* shader_string = str.c_str();
    const GLint   shader_string_length = static_cast<GLint>( str.length() );

    // Define o código do shader GLSL, contido na string "shader_string"
    glShaderSource(shader_id, 1, &shader_string, &shader_string_length);

    // Compila o código do shader GLSL (em tempo de execução)
    glCompileShader(shader_id);

    // Verificamos se ocorreu algum erro ou "warning" durante a compilação
    GLint compiled_ok;
    glGetShaderiv(shader_id, GL_COMPILE_STATUS, &compiled_ok);

    GLint log_length = 0;
    glGetShaderiv(shader_id, GL_INFO_LOG_LENGTH, &log_length);

    // Alocamos memória para guardar o log de compilação.
    // A chamada "new" em C++ é equivalente ao "malloc()" do C.
    GLchar* log = new GLchar[log_length];
    glGetShaderInfoLog(shader_id, log_length, &log_length, log);

    // Imprime no terminal qualquer erro ou "warning" de compilação
    if ( log_length != 0 )
    {
        std::string  output;

        if ( !compiled_ok )
        {
            output += "ERROR: OpenGL compilation of \"";
            output += filename;
            output += "\" failed.\n";
            output += "== Start of compilation log\n";
            output += log;
            output += "== End of compilation log\n";
        }
        else
        {
            output += "WARNING: OpenGL compilation of \"";
            output += filename;
            output += "\".\n";
            output += "== Start of compilation log\n";
            output += log;
            output += "== End of compilation log\n";
        }

        fprintf(stderr, "%s", output.c_str());
    }

    // A chamada "delete" em C++ é equivalente ao "free()" do C
    delete [] log;
}

// Esta função cria um programa de GPU, o qual contém obrigatoriamente um
// Vertex Shader e um Fragment Shader.
GLuint CreateGpuProgram(GLuint vertex_shader_id, GLuint fragment_shader_id)
{
    // Criamos um identificador (ID) para este programa de GPU
    GLuint program_id = glCreateProgram();

    // Definição dos dois shaders GLSL que devem ser executados pelo programa
    glAttachShader(program_id, vertex_shader_id);
    glAttachShader(program_id, fragment_shader_id);

    // Linkagem dos shaders acima ao programa
    glLinkProgram(program_id);

    // Verificamos se ocorreu algum erro durante a linkagem
    GLint linked_ok = GL_FALSE;
    glGetProgramiv(program_id, GL_LINK_STATUS, &linked_ok);

    // Imprime no terminal qualquer erro de linkagem
    if ( linked_ok == GL_FALSE )
    {
        GLint log_length = 0;
        glGetProgramiv(program_id, GL_INFO_LOG_LENGTH, &log_length);

        // Alocamos memória para guardar o log de compilação.
        // A chamada "new" em C++ é equivalente ao "malloc()" do C.
        GLchar* log = new GLchar[log_length];

        glGetProgramInfoLog(program_id, log_length, &log_length, log);

        std::string output;

        output += "ERROR: OpenGL linking of program failed.\n";
        output += "== Start of link log\n";
        output += log;
        output += "\n== End of link log\n";

        // A chamada "delete" em C++ é equivalente ao "free()" do C
        delete [] log;

        fprintf(stderr, "%s", output.c_str());
    }

    // Os "Shader Objects" podem ser marcados para deleção após serem linkados
    glDeleteShader(vertex_shader_id);
    glDeleteShader(fragment_shader_id);

    // Retornamos o ID gerado acima
    return program_id;
}

// Definição da função que será chamada sempre que a janela do sistema
// operacional for redimensionada, por consequência alterando o tamanho do
// "framebuffer" (região de memória onde são armazenados os pixels da imagem).
void FramebufferSizeCallback(GLFWwindow* window, int width, int height)
{
    // Indicamos que queremos renderizar em toda região do framebuffer. A
    // função "glViewport" define o mapeamento das "normalized device
    // coordinates" (NDC) para "pixel coordinates".  Essa é a operação de
    // "Screen Mapping" ou "Viewport Mapping" vista em aula (slides 33 até 42
    // do documento "Aula_07_Transformacoes_Geometricas_3D.pdf").
    glViewport(0, 0, width, height);

    // Atualizamos também a razão que define a proporção da janela (largura /
    // altura), a qual será utilizada na definição das matrizes de projeção,
    // tal que não ocorra distorções durante o processo de "Screen Mapping"
    // acima, quando NDC é mapeado para coordenadas de pixels. Veja slide 228
    // do documento "Aula_09_Projecoes.pdf".
    //
    // O cast para float é necessário pois números inteiros são arredondados ao
    // serem divididos!
    g_ScreenRatio = (float)width / height;
}

bool PlayerFloorColision(float floorY, float playerLowerY){
    if(playerLowerY <= floorY){
        return true;
    }
    return false;
}

// Variáveis globais que armazenam a última posição do cursor do mouse, para
// que possamos calcular quanto que o mouse se movimentou entre dois instantes
// de tempo. Utilizadas no callback CursorPosCallback() abaixo.
double g_LastCursorPosX, g_LastCursorPosY;

// Função callback chamada sempre que o usuário aperta algum dos botões do mouse
void MouseButtonCallback(GLFWwindow* window, int button, int action, int mods)
{
    if (button == GLFW_MOUSE_BUTTON_LEFT && action == GLFW_PRESS)
    {
        // Se o usuário pressionou o botão esquerdo do mouse, guardamos a
        // posição atual do cursor nas variáveis g_LastCursorPosX e
        // g_LastCursorPosY.  Também, setamos a variável
        // g_LeftMouseButtonPressed como true, para saber que o usuário está
        // com o botão esquerdo pressionado.

        glfwGetCursorPos(window, &g_LastCursorPosX, &g_LastCursorPosY);
        g_LeftMouseButtonPressed = true;

    }
    if (button == GLFW_MOUSE_BUTTON_LEFT && action == GLFW_RELEASE)
    {
        // Quando o usuário soltar o botão esquerdo do mouse, atualizamos a
        // variável abaixo para false.
        g_LeftMouseButtonPressed = false;
    }
    if (button == GLFW_MOUSE_BUTTON_RIGHT && action == GLFW_PRESS)
    {
        // Se o usuário pressionou o botão esquerdo do mouse, guardamos a
        // posição atual do cursor nas variáveis g_LastCursorPosX e
        // g_LastCursorPosY.  Também, setamos a variável
        // g_RightMouseButtonPressed como true, para saber que o usuário está
        // com o botão esquerdo pressionado.
        glfwGetCursorPos(window, &g_LastCursorPosX, &g_LastCursorPosY);
        g_RightMouseButtonPressed = true;
    }
    if (button == GLFW_MOUSE_BUTTON_RIGHT && action == GLFW_RELEASE)
    {
        // Quando o usuário soltar o botão esquerdo do mouse, atualizamos a
        // variável abaixo para false.
        g_RightMouseButtonPressed = false;
    }
    if (button == GLFW_MOUSE_BUTTON_MIDDLE && action == GLFW_PRESS)
    {
        // Se o usuário pressionou o botão esquerdo do mouse, guardamos a
        // posição atual do cursor nas variáveis g_LastCursorPosX e
        // g_LastCursorPosY.  Também, setamos a variável
        // g_MiddleMouseButtonPressed como true, para saber que o usuário está
        // com o botão esquerdo pressionado.
        glfwGetCursorPos(window, &g_LastCursorPosX, &g_LastCursorPosY);
        g_MiddleMouseButtonPressed = true;
    }
    if (button == GLFW_MOUSE_BUTTON_MIDDLE && action == GLFW_RELEASE)
    {
        // Quando o usuário soltar o botão esquerdo do mouse, atualizamos a
        // variável abaixo para false.
        g_MiddleMouseButtonPressed = false;
    }
}

// Função callback chamada sempre que o usuário movimentar o cursor do mouse em
// cima da janela OpenGL.
void CursorPosCallback(GLFWwindow* window, double xpos, double ypos)
{
    // Abaixo executamos o seguinte: caso o botão esquerdo do mouse esteja
    // pressionado, computamos quanto que o mouse se movimento desde o último
    // instante de tempo, e usamos esta movimentação para atualizar os
    // parâmetros que definem a posição da câmera dentro da cena virtual.
    // Assim, temos que o usuário consegue controlar a câmera.

    if (!g_LeftMouseButtonPressed)
        return;

    // Deslocamento do cursor do mouse em x e y de coordenadas de tela!
    float dx = xpos - g_LastCursorPosX;
    float dy = ypos - g_LastCursorPosY;

    // Atualizamos parâmetros da câmera com os deslocamentos
    camera_yaw   += 0.01f * dx;
    camera_pitch -= 0.01f * dy;

    // Em coordenadas esféricas, o ângulo phi deve ficar entre -pi/2 e +pi/2.
    float maxPitch = 3.141592f / 3;
    float minPitch = -maxPitch;
    float maxYaw = default_camera_yaw + 3.141592f / 3;
    float minYaw = default_camera_yaw - 3.141592f / 3;

    if (camera_pitch > maxPitch)
        camera_pitch = maxPitch;

    if (camera_pitch < minPitch)
        camera_pitch = minPitch;

    if (camera_yaw >= maxYaw)
        camera_yaw = maxYaw;

    if (camera_yaw < minYaw)
        camera_yaw = minYaw;

    // Atualizamos as variáveis globais para armazenar a posição atual do
    // cursor como sendo a última posição conhecida do cursor.
    camera_view_vector = glm::vec4(cos(camera_yaw) * cos(camera_pitch),
                                    sin(camera_pitch),
                                    sin(camera_yaw) * cos(camera_pitch),
                                    0.0f);
    g_LastCursorPosX = xpos;
    g_LastCursorPosY = ypos;
}

// Função callback chamada sempre que o usuário movimenta a "rodinha" do mouse.
void ScrollCallback(GLFWwindow* window, double xoffset, double yoffset)
{
    // Atualizamos a distância da câmera para a origem utilizando a
    // movimentação da "rodinha", simulando um ZOOM.
    g_CameraDistance -= 0.1f*yoffset;

    // Uma câmera look-at nunca pode estar exatamente "em cima" do ponto para
    // onde ela está olhando, pois isto gera problemas de divisão por zero na
    // definição do sistema de coordenadas da câmera. Isto é, a variável abaixo
    // nunca pode ser zero. Versões anteriores deste código possuíam este bug,
    // o qual foi detectado pelo aluno Vinicius Fraga (2017/2).
    const float verysmallnumber = std::numeric_limits<float>::epsilon();
    if (g_CameraDistance < verysmallnumber)
        g_CameraDistance = verysmallnumber;
}

// Definição da função que será chamada sempre que o usuário pressionar alguma
// tecla do teclado. Veja http://www.glfw.org/docs/latest/input_guide.html#input_key
void KeyCallback(GLFWwindow* window, int key, int scancode, int action, int mod)
{
    // Se o usuário pressionar a tecla ESC, fechamos a janela.
    if (key == GLFW_KEY_ESCAPE && action == GLFW_PRESS)
        glfwSetWindowShouldClose(window, GL_TRUE);

    // O código abaixo implementa a seguinte lógica:
    //   Se apertar tecla X       então g_AngleX += delta;
    //   Se apertar tecla shift+X então g_AngleX -= delta;
    //   Se apertar tecla Y       então g_AngleY += delta;
    //   Se apertar tecla shift+Y então g_AngleY -= delta;
    //   Se apertar tecla Z       então g_AngleZ += delta;
    //   Se apertar tecla shift+Z então g_AngleZ -= delta;

    float delta = 3.141592 / 16; // 22.5 graus, em radianos.

    if (key == GLFW_KEY_C && action == GLFW_PRESS)
    {
        free_cam_enabled = !free_cam_enabled;
    }

    if ((key == GLFW_KEY_LEFT || key == GLFW_KEY_A) && action == GLFW_PRESS){
        if(started && track > 0){
          //movement = 4;
          track--;
          timeWhenLeftPressed = glfwGetTime();
        }
    }
    if ((key == GLFW_KEY_RIGHT || key == GLFW_KEY_D) && action == GLFW_PRESS){
        if(started && track < 2){
          //movement = 3;
          track++;
          timeWhenRightPressed = glfwGetTime();
        }
    }

    // Se o usuário apertar a tecla espaço, resetamos os ângulos de Euler para zero.
    if ((key == GLFW_KEY_SPACE || key == GLFW_KEY_W || key == GLFW_KEY_UP) && action == GLFW_PRESS)
    {
        //g_AngleX = 0.0f;
        //g_AngleY = 0.0f;
        //g_AngleZ = 0.0f;
        //g_ForearmAngleX = 0.0f;
        //g_ForearmAngleZ = 0.0f;
        //g_TorsoPositionX = 0.0f;
        //g_TorsoPositionY = 0.0f;
        if(timeWhenSpacePressed == 0 && started){
            jumpStep = 1;
            movement = 1;
            spacePressed = true;
            timeWhenSpacePressed = glfwGetTime();
        }

        //g_TorsoPositionY = g_TorsoPositionY + 0.3f;

    }

    if (key == GLFW_KEY_ENTER && action == GLFW_PRESS)
    {
        if(!started){
            movement = 2;
            legUp = 'n';
        }
        started = true;
    }

    // Se o usuário apertar a tecla P, utilizamos projeção perspectiva.
    if (key == GLFW_KEY_P && action == GLFW_PRESS)
    {
        g_UsePerspectiveProjection = true;
    }

    // Se o usuário apertar a tecla O, utilizamos projeção ortográfica.
    if (key == GLFW_KEY_O && action == GLFW_PRESS)
    {
        g_UsePerspectiveProjection = false;
    }

    // Se o usuário apertar a tecla H, fazemos um "toggle" do texto informativo mostrado na tela.
    if (key == GLFW_KEY_H && action == GLFW_PRESS)
    {
        g_ShowInfoText = !g_ShowInfoText;
    }
}

// Definimos o callback para impressão de erros da GLFW no terminal
void ErrorCallback(int error, const char* description)
{
    fprintf(stderr, "ERROR: GLFW: %s\n", description);
}

// Esta função recebe um vértice com coordenadas de modelo p_model e passa o
// mesmo por todos os sistemas de coordenadas armazenados nas matrizes model,
// view, e projection; e escreve na tela as matrizes e pontos resultantes
// dessas transformações.
void TextRendering_ShowModelViewProjection(
    GLFWwindow* window,
    glm::mat4 projection,
    glm::mat4 view,
    glm::mat4 model,
    glm::vec4 p_model
)
{
    if ( !g_ShowInfoText )
        return;

    glm::vec4 p_world = model*p_model;
    glm::vec4 p_camera = view*p_world;

    float pad = TextRendering_LineHeight(window);

    TextRendering_PrintString(window, " Model matrix             Model     World", -1.0f, 1.0f-pad, 1.0f);
    TextRendering_PrintMatrixVectorProduct(window, model, p_model, -1.0f, 1.0f-2*pad, 1.0f);

    TextRendering_PrintString(window, " View matrix              World     Camera", -1.0f, 1.0f-7*pad, 1.0f);
    TextRendering_PrintMatrixVectorProduct(window, view, p_world, -1.0f, 1.0f-8*pad, 1.0f);

    TextRendering_PrintString(window, " Projection matrix        Camera                   NDC", -1.0f, 1.0f-13*pad, 1.0f);
    TextRendering_PrintMatrixVectorProductDivW(window, projection, p_camera, -1.0f, 1.0f-14*pad, 1.0f);
}

// Escrevemos na tela os ângulos de Euler definidos nas variáveis globais
// g_AngleX, g_AngleY, e g_AngleZ.
void TextRendering_ShowEulerAngles(GLFWwindow* window)
{
    if ( !g_ShowInfoText )
        return;

    float pad = TextRendering_LineHeight(window);

    char buffer[80];
    snprintf(buffer, 80, "Euler Angles rotation matrix = Z(%.2f)*Y(%.2f)*X(%.2f)\n", g_AngleZ, g_AngleY, g_AngleX);

    TextRendering_PrintString(window, buffer, -1.0f+pad/10, -1.0f+2*pad/10, 1.0f);
}

// Escrevemos na tela qual matriz de projeção está sendo utilizada.
void TextRendering_ShowProjection(GLFWwindow* window)
{
    if ( !g_ShowInfoText )
        return;

    float lineheight = TextRendering_LineHeight(window);
    float charwidth = TextRendering_CharWidth(window);

    if ( g_UsePerspectiveProjection )
        TextRendering_PrintString(window, "Perspective", 1.0f-13*charwidth, -1.0f+2*lineheight/10, 1.0f);
    else
        TextRendering_PrintString(window, "Orthographic", 1.0f-13*charwidth, -1.0f+2*lineheight/10, 1.0f);
}

// Escrevemos na tela o número de quadros renderizados por segundo (frames per
// second).
void TextRendering_ShowFramesPerSecond(GLFWwindow* window)
{
    if ( !g_ShowInfoText )
        return;

    // Variáveis estáticas (static) mantém seus valores entre chamadas
    // subsequentes da função!
    static float old_seconds = (float)glfwGetTime();
    static int   ellapsed_frames = 0;
    static char  buffer[20] = "?? fps";
    static int   numchars = 7;

    ellapsed_frames += 1;

    // Recuperamos o número de segundos que passou desde a execução do programa
    float seconds = (float)glfwGetTime();

    // Número de segundos desde o último cálculo do fps
    float ellapsed_seconds = seconds - old_seconds;

    if ( ellapsed_seconds > 1.0f )
    {
        numchars = snprintf(buffer, 20, "%.2f fps", ellapsed_frames / ellapsed_seconds);

        old_seconds = seconds;
        ellapsed_frames = 0;
    }

    float lineheight = TextRendering_LineHeight(window);
    float charwidth = TextRendering_CharWidth(window);

    TextRendering_PrintString(window, buffer, 1.0f-(numchars + 1)*charwidth, 1.0f-lineheight, 1.0f);
}

// set makeprg=cd\ ..\ &&\ make\ run\ >/dev/null
// vim: set spell spelllang=pt_br :
