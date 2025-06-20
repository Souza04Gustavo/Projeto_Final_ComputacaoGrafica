#define CGLM_FORCE_DEPTH_ZERO_TO_ONE // Opcional: se sua projeção for para Vulkan-style depth
// #define CGLM_LEFT_HANDED          // Opcional: se preferir coordenadas left-handed

#include <glad/glad.h>    // GLAD primeiro!
#include <GLFW/glfw3.h>

// Incluir cabeçalhos cglm
#include <cglm/cglm.h>    // Para a maioria das funções
#include <cglm/io.h>      // Para imprimir matrizes/vetores (útil para debug)

#include <stdio.h>        // Para printf
#include <stdlib.h>       // Para exit()

// Protótipos de Funções
void framebuffer_size_callback(GLFWwindow* window, int width, int height);
void processInput(GLFWwindow *window);
unsigned int compile_shader(const char* source, GLenum type);
unsigned int create_shader_program(const char* vertex_source, const char* fragment_source);

// Configurações
const unsigned int SCR_WIDTH = 800;
const unsigned int SCR_HEIGHT = 600;

// Shaders (os mesmos de antes)
const char *vertexShaderSource = "#version 330 core\n"
    "layout (location = 0) in vec3 aPos;\n"
    "uniform mat4 model;\n"
    "uniform mat4 view;\n"
    "uniform mat4 projection;\n"
    "void main()\n"
    "{\n"
    "   gl_Position = projection * view * model * vec4(aPos, 1.0);\n"
    "}\0";

const char *fragmentShaderSource = "#version 330 core\n"
    "out vec4 FragColor;\n"
    "uniform vec3 objectColor;\n" // Adicionando uma cor uniforme
    "void main()\n"
    "{\n"
    "   FragColor = vec4(objectColor, 1.0f);\n" // Usar a cor do objeto
    "}\0";


int main() {
    // GLFW: Inicialização e configuração
    if (!glfwInit()) {
        fprintf(stderr, "Falha ao inicializar GLFW\n");
        return -1;
    }
    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 3);
    glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);

#ifdef __APPLE__
    glfwWindowHint(GLFW_OPENGL_FORWARD_COMPAT, GL_TRUE);
#endif

    // GLFW: Criação da janela
    GLFWwindow* window = glfwCreateWindow(SCR_WIDTH, SCR_HEIGHT, "Meu Jogo CG (C Version)", NULL, NULL);
    if (window == NULL) {
        fprintf(stderr, "Falha ao criar janela GLFW\n");
        glfwTerminate();
        return -1;
    }
    glfwMakeContextCurrent(window);
    glfwSetFramebufferSizeCallback(window, framebuffer_size_callback);

    // GLAD: Carregar ponteiros de funções OpenGL
    if (!gladLoadGLLoader((GLADloadproc)glfwGetProcAddress)) {
        fprintf(stderr, "Falha ao inicializar GLAD\n");
        glfwTerminate(); // Terminar GLFW se GLAD falhar
        return -1;
    }

    // Criar Shader Program
    unsigned int shaderProgram = create_shader_program(vertexShaderSource, fragmentShaderSource);
    if (shaderProgram == 0) { // Checagem se o shader program foi criado com sucesso
        glfwTerminate();
        return -1;
    }


    // Configurar dados do vértice (e buffers) e atributos do vértice
    float vertices[] = {
        // posições
         0.5f,  0.5f, 0.0f,  // topo direita
         0.5f, -0.5f, 0.0f,  // baixo direita
        -0.5f, -0.5f, 0.0f,  // baixo esquerda
        -0.5f,  0.5f, 0.0f   // topo esquerda
    };
    unsigned int indices[] = {
        0, 1, 3,   // primeiro triangulo
        1, 2, 3    // segundo triangulo
    };

    unsigned int VBO, VAO, EBO;
    glGenVertexArrays(1, &VAO);
    glGenBuffers(1, &VBO);
    glGenBuffers(1, &EBO);

    glBindVertexArray(VAO);

    glBindBuffer(GL_ARRAY_BUFFER, VBO);
    glBufferData(GL_ARRAY_BUFFER, sizeof(vertices), vertices, GL_STATIC_DRAW);

    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, EBO);
    glBufferData(GL_ELEMENT_ARRAY_BUFFER, sizeof(indices), indices, GL_STATIC_DRAW);

    // Atributos do Vértice (posição)
    // layout (location = 0)
    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 3 * sizeof(float), (void*)0);
    glEnableVertexAttribArray(0);

    glBindBuffer(GL_ARRAY_BUFFER, 0);
    glBindVertexArray(0);


    // Matrizes de transformação com cglm
    mat4 model_m;          // Renomeado para evitar conflito com 'model' de C++
    mat4 view_m;
    mat4 projection_m;

    // Inicializar matrizes
    glm_mat4_identity(model_m);
    glm_mat4_identity(view_m);
    glm_mat4_identity(projection_m);

    // Configurar matriz de projeção ortográfica (para visualização 2D)
    // Coordenadas da tela: (0,0) no canto inferior esquerdo, (SCR_WIDTH, SCR_HEIGHT) no canto superior direito
    glm_ortho(0.0f, (float)SCR_WIDTH, 0.0f, (float)SCR_HEIGHT, -1.0f, 1.0f, projection_m);

    // Configurar matriz de visualização (câmera)
    // Para 2D, a câmera pode estar "olhando" para o plano Z=0 de uma posição Z=1
    // vec3 eye   = {SCR_WIDTH / 2.0f, SCR_HEIGHT / 2.0f, 1.0f}; // Câmera no centro olhando para frente
    // vec3 center = {SCR_WIDTH / 2.0f, SCR_HEIGHT / 2.0f, 0.0f};
    // vec3 up    = {0.0f, 1.0f, 0.0f};
    // glm_lookat(eye, center, up, view_m);
    // Ou, para 2D simples onde não movemos a câmera, a matriz de view pode ser a identidade se
    // a projeção já mapeia o espaço do mundo para o espaço da tela.
    // Por hora, vamos manter a view como identidade, já que a projeção ortográfica já define o espaço visível.
    // Se quisermos simular uma câmera se movendo sobre o plano 2D, ajustaremos a 'view_m'.

    // Loop de Renderização (Game Loop)
    while (!glfwWindowShouldClose(window)) {
        // Input
        processInput(window);

        // Renderização
        glClearColor(0.1f, 0.1f, 0.15f, 1.0f);
        glClear(GL_COLOR_BUFFER_BIT);

        glUseProgram(shaderProgram);

        // Definir a cor do objeto (exemplo: verde)
        vec3 object_color = {0.5f, 0.7f, 0.2f};
        glUniform3fv(glGetUniformLocation(shaderProgram, "objectColor"), 1, object_color);


        // Configurar matriz do modelo para o quadrado
        // Posicionar no centro da tela e dar um tamanho (ex: 100x100 pixels)
        glm_mat4_identity(model_m); // Começa com identidade
        vec3 square_pos = {SCR_WIDTH / 2.0f, SCR_HEIGHT / 2.0f, 0.0f};
        vec3 square_scale = {100.0f, 100.0f, 1.0f};
        glm_translate(model_m, square_pos);
        glm_scale(model_m, square_scale);
        // Se o quadrado tem coordenadas de -0.5 a 0.5, a escala o tornará de -50 a 50
        // e a translação o moverá.

        // Obter localizações dos uniforms
        unsigned int modelLoc = glGetUniformLocation(shaderProgram, "model");
        unsigned int viewLoc  = glGetUniformLocation(shaderProgram, "view");
        unsigned int projLoc  = glGetUniformLocation(shaderProgram, "projection");

        // Passar matrizes para o shader
        // cglm usa ponteiros diretos para as matrizes
        glUniformMatrix4fv(modelLoc, 1, GL_FALSE, (const GLfloat*)model_m);
        glUniformMatrix4fv(viewLoc, 1, GL_FALSE, (const GLfloat*)view_m);
        glUniformMatrix4fv(projLoc, 1, GL_FALSE, (const GLfloat*)projection_m);

        glBindVertexArray(VAO);
        glDrawElements(GL_TRIANGLES, 6, GL_UNSIGNED_INT, 0);

        glfwSwapBuffers(window);
        glfwPollEvents();
    }

    glDeleteVertexArrays(1, &VAO);
    glDeleteBuffers(1, &VBO);
    glDeleteBuffers(1, &EBO);
    glDeleteProgram(shaderProgram);

    glfwTerminate();
    return 0;
}

void framebuffer_size_callback(GLFWwindow* window, int width, int height) {
    glViewport(0, 0, width, height);
    // Atualizar projeção se a janela for redimensionada para manter o aspect ratio
    // ou o mapeamento de coordenadas desejado.
    // mat4 projection_m;
    // glm_ortho(0.0f, (float)width, 0.0f, (float)height, -1.0f, 1.0f, projection_m);
    // unsigned int shaderProgram; // você precisaria do ID do shader program aqui
    // glUseProgram(shaderProgram); // ativar o shader antes de setar o uniform
    // glUniformMatrix4fv(glGetUniformLocation(shaderProgram, "projection"), 1, GL_FALSE, (const GLfloat*)projection_m);
}

void processInput(GLFWwindow *window) {
    if (glfwGetKey(window, GLFW_KEY_ESCAPE) == GLFW_PRESS)
        glfwSetWindowShouldClose(window, true);
}

// Função para compilar um shader individual
unsigned int compile_shader(const char* source, GLenum type) {
    unsigned int shader = glCreateShader(type);
    glShaderSource(shader, 1, &source, NULL);
    glCompileShader(shader);

    int success;
    char infoLog[512];
    glGetShaderiv(shader, GL_COMPILE_STATUS, &success);
    if (!success) {
        glGetShaderInfoLog(shader, 512, NULL, infoLog);
        fprintf(stderr, "ERRO::SHADER::COMPILACAO_FALHOU\n%s\n", infoLog);
        if (type == GL_VERTEX_SHADER) fprintf(stderr, "Tipo: Vertex\n");
        else if (type == GL_FRAGMENT_SHADER) fprintf(stderr, "Tipo: Fragment\n");
        glDeleteShader(shader); // Não vazar o shader.
        return 0; // Retorna 0 em caso de erro
    }
    return shader;
}

// Função para criar um shader program a partir de fontes de vertex e fragment shader
unsigned int create_shader_program(const char* vertex_source, const char* fragment_source) {
    unsigned int vertexShader = compile_shader(vertex_source, GL_VERTEX_SHADER);
    if (vertexShader == 0) return 0;

    unsigned int fragmentShader = compile_shader(fragment_source, GL_FRAGMENT_SHADER);
    if (fragmentShader == 0) {
        glDeleteShader(vertexShader); // Limpar o vertex shader se o fragment falhar
        return 0;
    }

    unsigned int shaderProgram = glCreateProgram();
    glAttachShader(shaderProgram, vertexShader);
    glAttachShader(shaderProgram, fragmentShader);
    glLinkProgram(shaderProgram);

    int success;
    char infoLog[512];
    glGetProgramiv(shaderProgram, GL_LINK_STATUS, &success);
    if (!success) {
        glGetProgramInfoLog(shaderProgram, 512, NULL, infoLog);
        fprintf(stderr, "ERRO::SHADER::PROGRAMA::LINKAGEM_FALHOU\n%s\n", infoLog);
        glDeleteShader(vertexShader);   // Limpar shaders
        glDeleteShader(fragmentShader);
        glDeleteProgram(shaderProgram); // Limpar programa
        return 0; // Retorna 0 em caso de erro
    }

    // Os shaders não são mais necessários após a linkagem
    glDeleteShader(vertexShader);
    glDeleteShader(fragmentShader);

    return shaderProgram;
}