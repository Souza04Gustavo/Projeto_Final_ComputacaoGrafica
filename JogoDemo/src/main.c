#define CGLM_FORCE_DEPTH_ZERO_TO_ONE
// #define CGLM_LEFT_HANDED

#include <glad/glad.h>
#include <GLFW/glfw3.h>

#include <cglm/cglm.h>
#include <cglm/io.h>

#include <stdio.h>
#include <stdlib.h>

// --- Struct para o Jogador ---
typedef struct {
    vec2 position;
    vec2 size;
} Player;

// --- Configurações Globais ---
const unsigned int SCR_WIDTH = 800;
const unsigned int SCR_HEIGHT = 600;
const float PLAYER_SPEED = 250.0f; // Pixels por segundo (ajustei um pouco para cima)

// --- Protótipos de Funções ---
void framebuffer_size_callback(GLFWwindow* window, int width, int height);
void processInput(GLFWwindow *window, Player *player, float deltaTime);
unsigned int compile_shader(const char* source, GLenum type); // Declaração aqui
unsigned int create_shader_program(const char* vertex_source, const char* fragment_source); // Declaração aqui

// Variável global para o jogador
Player player;

// Shaders (mesmos de antes)
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
    "uniform vec3 objectColor;\n"
    "void main()\n"
    "{\n"
    "   FragColor = vec4(objectColor, 1.0f);\n"
    "}\0";

int main() {
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

    GLFWwindow* window = glfwCreateWindow(SCR_WIDTH, SCR_HEIGHT, "Meu Jogo CG (C Version)", NULL, NULL);
    if (window == NULL) {
        fprintf(stderr, "Falha ao criar janela GLFW\n");
        glfwTerminate();
        return -1;
    }
    glfwMakeContextCurrent(window);
    glfwSetFramebufferSizeCallback(window, framebuffer_size_callback);
    // Armazenar ponteiro da janela para uso no callback de redimensionamento, se necessário
    // glfwSetWindowUserPointer(window, &shaderProgramID); // Exemplo se precisasse do shader ID

    if (!gladLoadGLLoader((GLADloadproc)glfwGetProcAddress)) {
        fprintf(stderr, "Falha ao inicializar GLAD\n");
        glfwTerminate();
        return -1;
    }

    // Mover a criação do shader program para cá, para que seu ID esteja disponível para framebuffer_size_callback se necessário
    unsigned int shaderProgram = create_shader_program(vertexShaderSource, fragmentShaderSource);
    if (shaderProgram == 0) {
        glfwTerminate();
        return -1;
    }

    float vertices[] = {
         0.5f,  0.5f, 0.0f,
         0.5f, -0.5f, 0.0f,
        -0.5f, -0.5f, 0.0f,
        -0.5f,  0.5f, 0.0f
    };
    unsigned int indices[] = { 0, 1, 3, 1, 2, 3 };

    unsigned int VBO, VAO, EBO;
    glGenVertexArrays(1, &VAO);
    glGenBuffers(1, &VBO);
    glGenBuffers(1, &EBO);

    glBindVertexArray(VAO);
    glBindBuffer(GL_ARRAY_BUFFER, VBO);
    glBufferData(GL_ARRAY_BUFFER, sizeof(vertices), vertices, GL_STATIC_DRAW);
    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, EBO);
    glBufferData(GL_ELEMENT_ARRAY_BUFFER, sizeof(indices), indices, GL_STATIC_DRAW);
    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 3 * sizeof(float), (void*)0);
    glEnableVertexAttribArray(0);
    glBindBuffer(GL_ARRAY_BUFFER, 0);
    glBindVertexArray(0);

    player.position[0] = SCR_WIDTH / 2.0f;
    player.position[1] = SCR_HEIGHT / 2.0f;
    player.size[0] = 50.0f;
    player.size[1] = 50.0f;

    mat4 model_m;
    mat4 view_m;
    mat4 projection_m;

    glm_mat4_identity(view_m);
    glm_ortho(0.0f, (float)SCR_WIDTH, 0.0f, (float)SCR_HEIGHT, -1.0f, 1.0f, projection_m);

    float lastFrameTime = 0.0f;
    float currentFrameTime;
    float deltaTime;

    while (!glfwWindowShouldClose(window)) {
        currentFrameTime = (float)glfwGetTime();
        deltaTime = currentFrameTime - lastFrameTime;
        lastFrameTime = currentFrameTime;

        processInput(window, &player, deltaTime);

        glClearColor(0.1f, 0.1f, 0.15f, 1.0f);
        glClear(GL_COLOR_BUFFER_BIT);

        glUseProgram(shaderProgram);

        vec3 object_color = {0.5f, 0.7f, 0.2f};
        glUniform3fv(glGetUniformLocation(shaderProgram, "objectColor"), 1, object_color);

        glm_mat4_identity(model_m);
        glm_translate(model_m, (vec3){player.position[0], player.position[1], 0.0f});
        glm_scale(model_m, (vec3){player.size[0], player.size[1], 1.0f});

        unsigned int modelLoc = glGetUniformLocation(shaderProgram, "model");
        unsigned int viewLoc  = glGetUniformLocation(shaderProgram, "view");
        unsigned int projLoc  = glGetUniformLocation(shaderProgram, "projection");

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
    // Para atualizar a projeção ao redimensionar:
    // Você precisaria do ID do shader program. Uma forma é passá-lo via glfwSetWindowUserPointer
    // ou torná-lo uma variável global (menos ideal, mas funciona para projetos simples).
    // Supondo que 'shaderProgramID' é uma global ou obtida de alguma forma:
    /*
    if (shaderProgramID_global != 0) { // shaderProgramID_global seria o ID do nosso shader
        mat4 projection_new;
        glm_ortho(0.0f, (float)width, 0.0f, (float)height, -1.0f, 1.0f, projection_new);
        glUseProgram(shaderProgramID_global);
        glUniformMatrix4fv(glGetUniformLocation(shaderProgramID_global, "projection"), 1, GL_FALSE, (const GLfloat*)projection_new);
    }
    */
}

void processInput(GLFWwindow *window, Player *p, float deltaTime) {
    if (glfwGetKey(window, GLFW_KEY_ESCAPE) == GLFW_PRESS)
        glfwSetWindowShouldClose(window, true);

    float actual_speed = PLAYER_SPEED * deltaTime; // PLAYER_SPEED agora deve ser encontrado

    if (glfwGetKey(window, GLFW_KEY_W) == GLFW_PRESS)
        p->position[1] += actual_speed;
    if (glfwGetKey(window, GLFW_KEY_S) == GLFW_PRESS)
        p->position[1] -= actual_speed;
    if (glfwGetKey(window, GLFW_KEY_A) == GLFW_PRESS)
        p->position[0] -= actual_speed;
    if (glfwGetKey(window, GLFW_KEY_D) == GLFW_PRESS)
        p->position[0] += actual_speed;

    float half_width = p->size[0] / 2.0f;
    float half_height = p->size[1] / 2.0f;

    if (p->position[0] - half_width < 0.0f) p->position[0] = half_width;
    if (p->position[0] + half_width > SCR_WIDTH) p->position[0] = SCR_WIDTH - half_width;
    if (p->position[1] - half_height < 0.0f) p->position[1] = half_height;
    if (p->position[1] + half_height > SCR_HEIGHT) p->position[1] = SCR_HEIGHT - half_height;
}

// --- Definições das Funções de Shader (APENAS UMA VEZ) ---
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
        glDeleteShader(shader);
        return 0;
    }
    return shader;
}

unsigned int create_shader_program(const char* vertex_source, const char* fragment_source) {
    unsigned int vertexShader = compile_shader(vertex_source, GL_VERTEX_SHADER);
    if (vertexShader == 0) return 0;
    unsigned int fragmentShader = compile_shader(fragment_source, GL_FRAGMENT_SHADER);
    if (fragmentShader == 0) {
        glDeleteShader(vertexShader);
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
        glDeleteShader(vertexShader);
        glDeleteShader(fragmentShader);
        glDeleteProgram(shaderProgram);
        return 0;
    }
    glDeleteShader(vertexShader);
    glDeleteShader(fragmentShader);
    return shaderProgram;
}