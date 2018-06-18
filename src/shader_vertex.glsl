#version 330 core

// Atributos de vértice recebidos como entrada ("in") pelo Vertex Shader.
// Veja a função BuildTriangle() em "main.cpp".
layout (location = 0) in vec4 model_coefficients;
layout (location = 1) in vec4 color_coefficients;
layout (location = 2) in vec2 texture_coefficients;
layout (location = 3) in vec4 normal_coefficients;

// Atributos de vértice que serão gerados como saída ("out") pelo Vertex Shader.
// ** Estes serão interpolados pelo rasterizador! ** gerando, assim, valores
// para cada fragmento, os quais serão recebidos como entrada pelo Fragment
// Shader. Veja o arquivo "shader_fragment.glsl".
out vec4 interpColor;

// Matrizes computadas no código C++ e enviadas para a GPU
uniform mat4 model;
uniform mat4 view;
uniform mat4 projection;

out vec4 position_world;
out vec4 position_model;
out vec4 normal;
out vec2 texcoords;

// Variável booleana no código C++ também enviada para a GPU
uniform bool render_as_black;

void main()
{
    // A variável gl_Position define a posição final de cada vértice
    // OBRIGATORIAMENTE em "normalized device coordinates" (NDC), onde cada
    // coeficiente está entre -1 e 1.  (Veja slides 144 e 150 do documento
    // "Aula_09_Projecoes.pdf").
    //
    // O código em "main.cpp" define os vértices dos modelos em coordenadas
    // locais de cada modelo (array model_coefficients). Abaixo, utilizamos
    // operações de modelagem, definição da câmera, e projeção, para computar
    // as coordenadas finais em NDC (variável gl_Position). Após a execução
    // deste Vertex Shader, a placa de vídeo (GPU) fará a divisão por W. Veja
    // slide 189 do documento "Aula_09_Projecoes.pdf").

    gl_Position = projection * view * model * model_coefficients;

    // Como as variáveis acima  (tipo vec4) são vetores com 4 coeficientes,
    // também é possível acessar e modificar cada coeficiente de maneira
    // independente. Esses são indexados pelos nomes x, y, z, e w (nessa
    // ordem, isto é, 'x' é o primeiro coeficiente, 'y' é o segundo, ...):
    //
    //     gl_Position.x = model_coefficients.x;
    //     gl_Position.y = model_coefficients.y;
    //     gl_Position.z = model_coefficients.z;
    //     gl_Position.w = model_coefficients.w;
    //

        // Posição do vértice atual no sistema de coordenadas global (World).
    position_world = model * model_coefficients;

    // Posição do vértice atual no sistema de coordenadas local do modelo.
    position_model = model_coefficients;

    // Normal do vértice atual no sistema de coordenadas global (World).
    // Veja slide 94 do documento "Aula_07_Transformacoes_Geometricas_3D.pdf".
    normal = inverse(transpose(model)) * normal_coefficients;
    normal.w = 0.0;

    vec4 cam_pos = inverse(view) * vec4(0.0, 0.0, 0.0, 1.0);
    vec4 n = normalize(normal);

    // Vetor que define o sentido da fonte de luz em relação ao ponto atual.
    vec4 l = normalize(vec4(1.0,1.0,-1.0,0.0));

    // Vetor que define o sentido da câmera em relação ao ponto atual.
    vec4 v = normalize(cam_pos - position_world);

    // Vetor que define o sentido da reflexão especular ideal.
    vec4 r = -l + 2 * n * dot(n, l);

    vec4 I = vec4(1.0,1.0,1.0, 1.0);

    // Espectro da luz ambiente
    vec4 Ia = vec4(0.5, 0.5, 0.5, 1.0);

    // Termo difuso utilizando a lei dos cossenos de Lambert
    vec4 lambert_diffuse_term = color_coefficients * I * max(0, dot(n, v));

    // Termo ambiente
    vec4 ambient_term = color_coefficients * Ia;

    // Termo especular utilizando o modelo de iluminação de Phong
    vec4 phong_specular_term = vec4(0.8,0.8,0.8,1.0) * I * pow(max(0, dot(r, v)), 20);

    // Coordenadas de textura obtidas do arquivo OBJ (se existirem!)
    texcoords = texture_coefficients;

    if ( render_as_black )
    {
        // Ignoramos o atributo cor dos vértices, colocando a cor final como
        // preta. Utilizamos isto para renderizar as arestas pretas dos cubos.
        interpColor = vec4(0.0f,0.0f,0.0f,1.0f);
    }
    else
    {
        // Copiamos o atributo cor (de entrada) de cada vértice para a variável
        // "cor_interpolada_pelo_rasterizador". Esta variável será interpolada pelo
        // rasterizador, gerando valores interpolados para cada fragmento!  Veja o
        // arquivo "shader_fragment.glsl".
        interpColor = lambert_diffuse_term + ambient_term + phong_specular_term;
    }
}
