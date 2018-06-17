#version 330 core

// Atributos de v�rtice recebidos como entrada ("in") pelo Vertex Shader.
// Veja a fun��o BuildTriangle() em "main.cpp".
layout (location = 0) in vec4 model_coefficients;
layout (location = 1) in vec4 color_coefficients;
layout (location = 2) in vec2 texture_coefficients;
layout (location = 3) in vec4 normal_coefficients;

// Atributos de v�rtice que ser�o gerados como sa�da ("out") pelo Vertex Shader.
// ** Estes ser�o interpolados pelo rasterizador! ** gerando, assim, valores
// para cada fragmento, os quais ser�o recebidos como entrada pelo Fragment
// Shader. Veja o arquivo "shader_fragment.glsl".
out vec4 interpColor;

// Matrizes computadas no c�digo C++ e enviadas para a GPU
uniform mat4 model;
uniform mat4 view;
uniform mat4 projection;

out vec4 position_world;
out vec4 position_model;
out vec4 normal;
out vec2 texcoords;

// Vari�vel booleana no c�digo C++ tamb�m enviada para a GPU
uniform bool render_as_black;

void main()
{
    // A vari�vel gl_Position define a posi��o final de cada v�rtice
    // OBRIGATORIAMENTE em "normalized device coordinates" (NDC), onde cada
    // coeficiente est� entre -1 e 1.  (Veja slides 144 e 150 do documento
    // "Aula_09_Projecoes.pdf").
    //
    // O c�digo em "main.cpp" define os v�rtices dos modelos em coordenadas
    // locais de cada modelo (array model_coefficients). Abaixo, utilizamos
    // opera��es de modelagem, defini��o da c�mera, e proje��o, para computar
    // as coordenadas finais em NDC (vari�vel gl_Position). Ap�s a execu��o
    // deste Vertex Shader, a placa de v�deo (GPU) far� a divis�o por W. Veja
    // slide 189 do documento "Aula_09_Projecoes.pdf").

    gl_Position = projection * view * model * model_coefficients;

    // Como as vari�veis acima  (tipo vec4) s�o vetores com 4 coeficientes,
    // tamb�m � poss�vel acessar e modificar cada coeficiente de maneira
    // independente. Esses s�o indexados pelos nomes x, y, z, e w (nessa
    // ordem, isto �, 'x' � o primeiro coeficiente, 'y' � o segundo, ...):
    //
    //     gl_Position.x = model_coefficients.x;
    //     gl_Position.y = model_coefficients.y;
    //     gl_Position.z = model_coefficients.z;
    //     gl_Position.w = model_coefficients.w;
    //

        // Posi��o do v�rtice atual no sistema de coordenadas global (World).
    position_world = model * model_coefficients;

    // Posi��o do v�rtice atual no sistema de coordenadas local do modelo.
    position_model = model_coefficients;

    // Normal do v�rtice atual no sistema de coordenadas global (World).
    // Veja slide 94 do documento "Aula_07_Transformacoes_Geometricas_3D.pdf".
    normal = inverse(transpose(model)) * normal_coefficients;
    normal.w = 0.0;

    vec4 cam_pos = inverse(view) * vec4(0.0, 0.0, 0.0, 1.0);
    vec4 n = normalize(normal);

    // Vetor que define o sentido da fonte de luz em rela��o ao ponto atual.
    vec4 l = normalize(vec4(1.0,1.0,-1.0,0.0));

    // Vetor que define o sentido da c�mera em rela��o ao ponto atual.
    vec4 v = normalize(cam_pos - position_world);

    // Vetor que define o sentido da reflex�o especular ideal.
    vec4 r = -l + 2 * n * dot(n, l);

    vec4 I = vec4(1.0,1.0,1.0, 1.0);

    // Espectro da luz ambiente
    vec4 Ia = vec4(0.5, 0.5, 0.5, 1.0);

    // Termo difuso utilizando a lei dos cossenos de Lambert
    vec4 lambert_diffuse_term = color_coefficients * I * max(0, dot(n, v));

    // Termo ambiente
    vec4 ambient_term = color_coefficients * Ia;

    // Termo especular utilizando o modelo de ilumina��o de Phong
    vec4 phong_specular_term = vec4(0.8,0.8,0.8,1.0) * I * pow(max(0, dot(r, v)), 20);

    // Coordenadas de textura obtidas do arquivo OBJ (se existirem!)
    texcoords = texture_coefficients;

    if ( render_as_black )
    {
        // Ignoramos o atributo cor dos v�rtices, colocando a cor final como
        // preta. Utilizamos isto para renderizar as arestas pretas dos cubos.
        interpColor = vec4(0.0f,0.0f,0.0f,1.0f);
    }
    else
    {
        // Copiamos o atributo cor (de entrada) de cada v�rtice para a vari�vel
        // "cor_interpolada_pelo_rasterizador". Esta vari�vel ser� interpolada pelo
        // rasterizador, gerando valores interpolados para cada fragmento!  Veja o
        // arquivo "shader_fragment.glsl".
        interpColor = lambert_diffuse_term + ambient_term + phong_specular_term;
    }
}
