#define CGLM_FORCE_DEPTH_ZERO_TO_ONE
#define STB_IMAGE_IMPLEMENTATION

#include <stb/stb_image.h> // Ajuste o caminho se necessário
#include <glad/glad.h>
#include <GLFW/glfw3.h>
#include <cglm/cglm.h>
#include <cglm/io.h>
#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include <math.h> // Para sqrtf e atan2f (se precisar de ângulo)
#include <stdbool.h> // Para bool, true, false

#define MAX_SLIMES 20       // Número máximo de slimes que podem existir ao mesmo tempo
#define SLIME_SPEED 150.0f  // Velocidade dos slimes

#define MAX_PROJECTILES 50
#define PROJECTILE_SPEED 500.0f
#define PROJECTILE_SIZE 10.0f
#define PROJECTILE_LIFETIME 2.0f // Segundos
#define FIRE_RATE 0.25f // Segundos entre disparos (4 tiros por segundo)
#define PROJECTILE_DAMAGE 25.0f // Cada tiro tira 25 de vida
#define SLIME_MAX_HEALTH 100.0f // Slimes começam com 100 de vida
#define PLAYER_MAX_HEALTH 100.0f
#define SLIME_COLLISION_DAMAGE 25.0f // Dano que o slime causa ao colidir


const unsigned int SCR_WIDTH = 800;
const unsigned int SCR_HEIGHT = 600;
const float PLAYER_SPEED = 250.0f; // Pixels por segundo (ajustei um pouco para cima)
const float PLAYER_ROTATION_SPEED = 3.0f; // Radianos por segundo (ajuste conforme necessário)
// const char *caminho_background = "shaders/chao/Ground_01.png"; // Caminho para a textura de fundo
const char *caminho_background = "shaders/chao/tunnel_road.jpg"; // Caminho para a textura de fundo
const char *caminho_player = "shaders/player/handgun/idle/survivor-idle_handgun_1.png"; // Caminho para a textura do jogador
const char *caminho_game_over = "shaders/interfaces/gameover.png"; // Caminho para a imagem

typedef struct {
    vec2 position;
    vec2 size;
    float angle;
    float health;
    float maxHealth;
} Player;

typedef struct {
    float x, y, z;      // Posição no mundo 3D
    float raio;         // Tamanho do slime
    float cor[3];       // Cor RGB (ex: {1.0f, 0.5f, 0.0f} para laranja)
    bool ativo;         // Para saber se o slime está "em jogo"
    float health;
    float maxHealth;
} Slime;


typedef struct {
    vec2 position;
    vec2 velocity; // Embora possamos calcular a partir da direção e velocidade_escalar
    float speed;
    float angle;      // Ângulo em que foi disparado (para saber sua direção)
    float size;       // Tamanho do projétil (ex: lado de um quadrado)
    bool active;
    float lifetime;   // Opcional: quanto tempo o projétil vive antes de desaparecer
} Projectile;


typedef enum {
    GAME_STATE_PLAYING,
    GAME_STATE_GAMEOVER
} GameState;


void framebuffer_size_callback(GLFWwindow* window, int width, int height);
void processInput(GLFWwindow *window, Player *player, float deltaTime);
unsigned int compile_shader(const char* source, GLenum type);
unsigned int create_shader_program(const char* vertex_source, const char* fragment_source);
void initializeSlimes();
void spawnSlime();
void updateSlimes(float deltaTime, Player* player);
void drawSlimes(unsigned int gameObjectShaderProgram, unsigned int gameObjectVAO, mat4 model_m_ref);
void initializeProjectiles();
void fireProjectile(Player* p); // Passa o jogador para saber de onde e para onde atirar
void updateProjectiles(float deltaTime);
void drawProjectiles(unsigned int currentShaderProgram, unsigned int currentVAO, mat4 model_matrix_ref_placeholder);
unsigned int loadTexture(const char *path); // Nova função
void setupBackgroundGeometry(); // Adicionado protótipo
void resetGame();
void drawText(unsigned int shaderProgram, unsigned int vao, const char* text, float x, float y, float scale);
void drawCharacter(unsigned int shaderProgram, unsigned int vao, char character, float x, float y, float scale);

Player player;
Slime slimes[MAX_SLIMES];
Projectile projectiles[MAX_PROJECTILES];
GameState currentGameState = GAME_STATE_PLAYING;
unsigned int playerTextureID; // ID da textura do jogador
int activeProjectilesCount = 0; // Se você não for compactar o array, este não será usado assim
int activeSlimesCount = 0; // Contador de slimes atualmente ativos
float timeSinceLastShot = 0.0f;
float timeSinceLastSpawn = 0.0f;

// Recursos do Fundo
unsigned int backgroundTextureID;
unsigned int gameOverTextureID; // ID para a textura da tela de Game Over
unsigned int backgroundVAO, backgroundVBO, backgroundEBO;
unsigned int backgroundShaderProgram;

// Shaders para OBJETOS DO JOGO (agora com suporte a textura opcional)
const char *vertexShaderSource = "#version 330 core\n"
    "layout (location = 0) in vec3 aPos;\n"
    "layout (location = 1) in vec2 aTexCoord_Object;\n" // Adicionar entrada para coords de textura
    "uniform mat4 model;\n"
    "uniform mat4 view;\n"
    "uniform mat4 projection;\n"
    "out vec2 TexCoord_Object;\n" // Passar para o fragment shader
    "void main()\n"
    "{\n"
    "   gl_Position = projection * view * model * vec4(aPos, 1.0);\n"
    "   TexCoord_Object = aTexCoord_Object;\n"
    "}\0";

const char *fragmentShaderSource = "#version 330 core\n"
    "out vec4 FragColor;\n"
    "in vec2 TexCoord_Object;\n"      // Receber coords de textura
    "uniform vec3 objectColor_Solid;\n" // Renomeado para clareza
    "uniform sampler2D objectTexture_Sampler;\n" // Sampler para a textura do objeto
    "uniform bool useTexture;\n"         // Flag para decidir se usa textura ou cor sólida
    "void main()\n"
    "{\n"
    "   if (useTexture) {\n"
    "       vec4 texColor = texture(objectTexture_Sampler, TexCoord_Object);\n"
    "       if(texColor.a < 0.1) discard; // Descartar pixels transparentes (opcional, mas bom para sprites)\n"
    "       FragColor = texColor;\n"
    "   } else {\n"
    "       FragColor = vec4(objectColor_Solid, 1.0f);\n"
    "   }\n"
    "}\0";

// Shaders para o FUNDO (texturizado) - MOVENDO PARA ESCOPO GLOBAL
const char *backgroundVertexShaderSource_global = "#version 330 core\n" // Renomeado para evitar conflito se houvesse local
    "layout (location = 0) in vec3 aPos;\n"
    "layout (location = 1) in vec2 aTexCoord;\n"
    "out vec2 TexCoord;\n"
    "uniform mat4 model;\n"
    "uniform mat4 view;\n"
    "uniform mat4 projection;\n"
    "void main()\n"
    "{\n"
    "   gl_Position = projection * view * model * vec4(aPos, 1.0);\n"
    "   TexCoord = aTexCoord;\n"
    "}\0";

const char *backgroundFragmentShaderSource_global = "#version 330 core\n" // Renomeado
    "out vec4 FragColor;\n"
    "in vec2 TexCoord;\n"
    "uniform sampler2D backgroundTexture;\n"
    "void main()\n"
    "{\n"
    "   FragColor = texture(backgroundTexture, TexCoord);\n"
    "}\0";

int main() {
    srand(time(NULL));

    if (!glfwInit()) { fprintf(stderr, "Falha GLFW\n"); return -1; }
    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 3);
    glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);
    #ifdef __APPLE__
    glfwWindowHint(GLFW_OPENGL_FORWARD_COMPAT, GL_TRUE);
    #endif

    GLFWwindow* window = glfwCreateWindow(SCR_WIDTH, SCR_HEIGHT, "Jogo 2D", NULL, NULL);
    if (window == NULL) { fprintf(stderr, "Falha Janela\n"); glfwTerminate(); return -1; }

    glfwSetInputMode(window, GLFW_CURSOR, GLFW_CURSOR_NORMAL);
    glfwMakeContextCurrent(window);
    glfwSetFramebufferSizeCallback(window, framebuffer_size_callback);

    if (!gladLoadGLLoader((GLADloadproc)glfwGetProcAddress)) { fprintf(stderr, "Falha GLAD\n"); glfwTerminate(); return -1; }

    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

    // --- Shader e VAO para os OBJETOS DO JOGO (suporta textura e cor sólida) ---
    unsigned int gameObjectShaderProgram = create_shader_program(vertexShaderSource, fragmentShaderSource);
    if (gameObjectShaderProgram == 0) { glfwTerminate(); return -1; }

    // VAO para todos os objetos (jogador, slimes, projéteis, arma)
    unsigned int gameObjectVAO;
    unsigned int gameObjectVBO, gameObjectEBO;
    float gameObjectVertices[] = {
        // Posições         // Coords Textura
         0.5f,  0.5f, 0.0f,  1.0f, 1.0f,
         0.5f, -0.5f, 0.0f,  1.0f, 0.0f,
        -0.5f, -0.5f, 0.0f,  0.0f, 0.0f,
        -0.5f,  0.5f, 0.0f,  0.0f, 1.0f
    };
    unsigned int gameObjectIndices[] = { 0, 1, 3, 1, 2, 3 };

    glGenVertexArrays(1, &gameObjectVAO);
    glGenBuffers(1, &gameObjectVBO);
    glGenBuffers(1, &gameObjectEBO);

    // CORREÇÃO CRÍTICA AQUI:
    glBindVertexArray(gameObjectVAO); // Vincular o VAO

    glBindBuffer(GL_ARRAY_BUFFER, gameObjectVBO);
    glBufferData(GL_ARRAY_BUFFER, sizeof(gameObjectVertices), gameObjectVertices, GL_STATIC_DRAW);
    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, gameObjectEBO);
    glBufferData(GL_ELEMENT_ARRAY_BUFFER, sizeof(gameObjectIndices), gameObjectIndices, GL_STATIC_DRAW);

    // Atributo de Posição (layout=0), Stride é 5 (3 pos + 2 tex)
    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 5 * sizeof(float), (void*)0);
    glEnableVertexAttribArray(0);

    // Atributo de Coordenada de Textura (layout=1), Stride é 5, Offset é 3
    glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, 5 * sizeof(float), (void*)(3 * sizeof(float)));
    glEnableVertexAttribArray(1);

    glBindVertexArray(0); // Desvincular APÓS configurar todos os atributos

    // Carregar texturas
    playerTextureID = loadTexture(caminho_player);
    if (playerTextureID == 0) { fprintf(stderr, "Textura do jogador não carregada!\n"); }

    // --- Shader, Textura e VAO para o FUNDO ---
    backgroundShaderProgram = create_shader_program(backgroundVertexShaderSource_global, backgroundFragmentShaderSource_global);
    if (backgroundShaderProgram == 0) { glfwTerminate(); return -1; }

    backgroundTextureID = loadTexture(caminho_background);
    if (backgroundTextureID == 0) { fprintf(stderr, "Textura de fundo nao carregada, saindo.\n"); glfwTerminate(); return -1; }
    setupBackgroundGeometry();

    gameOverTextureID = loadTexture(caminho_game_over);
    if (gameOverTextureID == 0) { fprintf(stderr, "Textura de Game Over não carregada!\n"); }

    // Inicializações do Jogo
    player.position[0] = SCR_WIDTH / 2.0f;
    player.position[1] = SCR_HEIGHT / 2.0f;
    player.size[0] = 50.0f;
    player.size[1] = 50.0f;
    player.angle = 0.0f;
    player.maxHealth = PLAYER_MAX_HEALTH;
    player.health = PLAYER_MAX_HEALTH;
    initializeSlimes();
    initializeProjectiles();

    mat4 view_m, projection_m;
    glm_mat4_identity(view_m);
    glm_ortho(0.0f, (float)SCR_WIDTH, 0.0f, (float)SCR_HEIGHT, -1.0f, 1.0f, projection_m);

    float lastFrameTime = 0.0f, deltaTime;
    float spawnInterval = 3.0f; // Intervalo de spawn de slimes em segundos

    // Localizações de Uniforms
    unsigned int go_modelLoc = glGetUniformLocation(gameObjectShaderProgram, "model");
    unsigned int go_viewLoc  = glGetUniformLocation(gameObjectShaderProgram, "view");
    unsigned int go_projLoc  = glGetUniformLocation(gameObjectShaderProgram, "projection");
    unsigned int go_colorSolidLoc = glGetUniformLocation(gameObjectShaderProgram, "objectColor_Solid"); // Nome correto do uniform
    unsigned int go_useTextureLoc = glGetUniformLocation(gameObjectShaderProgram, "useTexture");
    unsigned int go_objectTextureSamplerLoc = glGetUniformLocation(gameObjectShaderProgram, "objectTexture_Sampler");

    unsigned int bg_modelLoc = glGetUniformLocation(backgroundShaderProgram, "model");
    unsigned int bg_viewLoc  = glGetUniformLocation(backgroundShaderProgram, "view");
    unsigned int bg_projLoc  = glGetUniformLocation(backgroundShaderProgram, "projection");
    unsigned int bg_textureSamplerLoc = glGetUniformLocation(backgroundShaderProgram, "backgroundTexture");

    // Loop Principal
    while (!glfwWindowShouldClose(window)) {
        float currentFrameTime = (float)glfwGetTime();
        deltaTime = currentFrameTime - lastFrameTime;
        lastFrameTime = currentFrameTime;

        processInput(window, &player, deltaTime);
        glfwPollEvents();

        // --- LIMPAR A TELA ---
        glClearColor(0.1f, 0.1f, 0.15f, 1.0f);
        glClear(GL_COLOR_BUFFER_BIT);

    if(currentGameState == GAME_STATE_PLAYING){

        timeSinceLastSpawn += deltaTime; // Incrementar o contador de tempo
        if (timeSinceLastSpawn >= spawnInterval) {
            spawnSlime(); // Tenta spawnar um slime
            timeSinceLastSpawn = 0.0f; // Reseta o contador
        }

        updateSlimes(deltaTime, &player);
        updateProjectiles(deltaTime);

        glClearColor(0.1f, 0.1f, 0.15f, 1.0f);
        glClear(GL_COLOR_BUFFER_BIT);

        // --- 1. DESENHAR O FUNDO ---
        glUseProgram(backgroundShaderProgram);
        glUniformMatrix4fv(bg_viewLoc, 1, GL_FALSE, (const GLfloat*)view_m);
        glUniformMatrix4fv(bg_projLoc, 1, GL_FALSE, (const GLfloat*)projection_m);
        mat4 model_background;
        glm_mat4_identity(model_background);
        glUniformMatrix4fv(bg_modelLoc, 1, GL_FALSE, (const GLfloat*)model_background);
        glActiveTexture(GL_TEXTURE0);
        glBindTexture(GL_TEXTURE_2D, backgroundTextureID);
        glUniform1i(bg_textureSamplerLoc, 0);
        glBindVertexArray(backgroundVAO);
        glDrawElements(GL_TRIANGLES, 6, GL_UNSIGNED_INT, 0);

        // --- 2. DESENHAR OBJETOS DO JOGO ---
        glUseProgram(gameObjectShaderProgram);
        glUniformMatrix4fv(go_viewLoc, 1, GL_FALSE, (const GLfloat*)view_m);
        glUniformMatrix4fv(go_projLoc, 1, GL_FALSE, (const GLfloat*)projection_m);
        glBindVertexArray(gameObjectVAO);

        // --- Desenhar Jogador (com textura) ---
        glUniform1i(go_useTextureLoc, true); // Dizer ao shader para usar textura
        glActiveTexture(GL_TEXTURE0); // Pode reutilizar a unidade de textura
        glBindTexture(GL_TEXTURE_2D, playerTextureID);
        glUniform1i(go_objectTextureSamplerLoc, 0);

        mat4 model_player;
        glm_mat4_identity(model_player);
        glm_translate(model_player, (vec3){player.position[0], player.position[1], 0.0f});
        glm_rotate_z(model_player, player.angle, model_player);
        float player_visual_scale_factor = 1.0f; // Ajuste o tamanho visual aqui
        glm_scale(model_player, (vec3){player.size[0] * player_visual_scale_factor, player.size[1] * player_visual_scale_factor, 1.0f});
        glUniformMatrix4fv(go_modelLoc, 1, GL_FALSE, (const GLfloat*)model_player);
        glDrawElements(GL_TRIANGLES, 6, GL_UNSIGNED_INT, 0);

        // --- Desenhar Arma, Projéteis e Slimes (com cor sólida) ---
        glUniform1i(go_useTextureLoc, false); // Dizer ao shader para usar cor sólida para o resto

        // Desenhar Arma
        mat4 model_weapon;
        vec3 weapon_color_vec = {0.5f, 0.5f, 0.5f};
        glUniform3fv(go_colorSolidLoc, 1, weapon_color_vec);
        glm_mat4_identity(model_weapon);
        glm_translate(model_weapon, (vec3){player.position[0], player.position[1], 0.0f});
        glm_rotate_z(model_weapon, player.angle, model_weapon);
        float weapon_offset_distance = player.size[0] * 0.4f;
        glm_translate(model_weapon, (vec3){weapon_offset_distance, 0.0f, 0.0f});
        float weapon_width = player.size[0] * 0.6f;
        float weapon_height = player.size[1] * 0.2f;
        glm_scale(model_weapon, (vec3){weapon_width, weapon_height, 1.0f});
        glUniformMatrix4fv(go_modelLoc, 1, GL_FALSE, (const GLfloat*)model_weapon);
        glDrawElements(GL_TRIANGLES, 6, GL_UNSIGNED_INT, 0);

        // Desenhar Projéteis
        drawProjectiles(gameObjectShaderProgram, gameObjectVAO, model_player); // Passar os recursos corretos
        // Desenhar Slimes
        drawSlimes(gameObjectShaderProgram, gameObjectVAO, model_player); // Passar os recursos corretos

        // --- 3. DESENHAR A INTERFACE DO USUÁRIO (HUD) ---
        // A HUD deve ser desenhada por último para ficar na frente
        // Usaremos o mesmo shader e VAO dos objetos, pois são apenas retângulos coloridos.
        glUseProgram(gameObjectShaderProgram);
        glBindVertexArray(gameObjectVAO);
        
        // Definições da Barra de Vida da HUD
        float hud_bar_width = 200.0f;
        float hud_bar_height = 20.0f;
        float hud_bar_padding = 10.0f;
        float hud_bar_x = SCR_WIDTH - hud_bar_width - hud_bar_padding;
        float hud_bar_y = SCR_HEIGHT - hud_bar_height - hud_bar_padding;

        // Desenhar fundo da barra de vida (vermelha)
        vec3 hud_bar_bg_color = {1.0f, 0.0f, 0.0f};
        glUniform1i(go_useTextureLoc, false); // Garantir que estamos usando cor sólida
        glUniform3fv(go_colorSolidLoc, 1, hud_bar_bg_color);

        mat4 model_hud_bg;
        glm_mat4_identity(model_hud_bg);
        // Nosso VAO é um quadrado unitário (-0.5 a 0.5), então seu centro é (0,0).
        // Transladamos para o centro da barra que queremos desenhar.
        glm_translate(model_hud_bg, (vec3){hud_bar_x + hud_bar_width / 2.0f, hud_bar_y + hud_bar_height / 2.0f, 0.0f});
        glm_scale(model_hud_bg, (vec3){hud_bar_width, hud_bar_height, 1.0f});
        glUniformMatrix4fv(go_modelLoc, 1, GL_FALSE, (const GLfloat*)model_hud_bg);
        glDrawElements(GL_TRIANGLES, 6, GL_UNSIGNED_INT, 0);

        // Desenhar frente da barra de vida (verde)
        float health_percentage = player.health / player.maxHealth;
        if (health_percentage < 0) health_percentage = 0;

        float green_bar_width = hud_bar_width * health_percentage;

        if (green_bar_width > 0) { // Só desenha se houver vida
            vec3 hud_bar_fg_color = {0.0f, 1.0f, 0.0f};
            glUniform3fv(go_colorSolidLoc, 1, hud_bar_fg_color);

            mat4 model_hud_fg;
            glm_mat4_identity(model_hud_fg);
            // Transladar para o centro da parte verde da barra, alinhada à esquerda
            float green_bar_center_x = hud_bar_x + (green_bar_width / 2.0f);
            glm_translate(model_hud_fg, (vec3){green_bar_center_x, hud_bar_y + hud_bar_height / 2.0f, 0.0f});
            glm_scale(model_hud_fg, (vec3){green_bar_width, hud_bar_height, 1.0f});
            glUniformMatrix4fv(go_modelLoc, 1, GL_FALSE, (const GLfloat*)model_hud_fg);
            glDrawElements(GL_TRIANGLES, 6, GL_UNSIGNED_INT, 0);
        }

    }
    else if (currentGameState == GAME_STATE_GAMEOVER) {
            // ==========================================================
            // RENDERIZAÇÃO DA TELA DE GAME OVER (MUITO MAIS SIMPLES AGORA)
            // ==========================================================

            // 1. Redesenha o fundo do jogo para que ele apareça atrás da tela de Game Over.
            glUseProgram(backgroundShaderProgram);
            glBindTexture(GL_TEXTURE_2D, backgroundTextureID);
            glBindVertexArray(backgroundVAO);
            mat4 model_bg;
            glm_mat4_identity(model_bg);
            glUniformMatrix4fv(bg_modelLoc, 1, GL_FALSE, (const GLfloat*)model_bg);
            glDrawElements(GL_TRIANGLES, 6, GL_UNSIGNED_INT, 0);

            // 2. Desenha a imagem de Game Over no centro da tela.
            glUseProgram(gameObjectShaderProgram); // Usamos o shader de objetos que suporta textura
            
            // Configura para usar textura
            glUniform1i(go_useTextureLoc, true);
            glActiveTexture(GL_TEXTURE0);
            glBindTexture(GL_TEXTURE_2D, gameOverTextureID);
            glUniform1i(go_objectTextureSamplerLoc, 0);

            // Define o tamanho da imagem de Game Over na tela
            float gameOverImageWidth = 600.0f;  // Ajuste para a largura da sua imagem
            float gameOverImageHeight = 300.0f; // Ajuste para a altura da sua imagem

            mat4 model_gameover;
            glm_mat4_identity(model_gameover);
            
            // Translada para o centro da tela
            glm_translate(model_gameover, (vec3){SCR_WIDTH / 2.0f, SCR_HEIGHT / 2.0f, 0.0f});
            // Escala o nosso quadrado unitário para o tamanho da imagem
            glm_scale(model_gameover, (vec3){gameOverImageWidth, gameOverImageHeight, 1.0f});

            glUniformMatrix4fv(go_modelLoc, 1, GL_FALSE, (const GLfloat*)model_gameover);
            
            glBindVertexArray(gameObjectVAO); // Reutilizamos o VAO dos objetos
            glDrawElements(GL_TRIANGLES, 6, GL_UNSIGNED_INT, 0);
        }

    glfwSwapBuffers(window);

    }
    // Cleanup
    glDeleteVertexArrays(1, &gameObjectVAO);
    glDeleteBuffers(1, &gameObjectVBO);
    glDeleteBuffers(1, &gameObjectEBO);
    glDeleteProgram(gameObjectShaderProgram);

    glDeleteVertexArrays(1, &backgroundVAO);
    glDeleteBuffers(1, &backgroundVBO);
    glDeleteBuffers(1, &backgroundEBO);
    glDeleteProgram(backgroundShaderProgram);
    glDeleteTextures(1, &backgroundTextureID);
    if(playerTextureID > 0) glDeleteTextures(1, &playerTextureID);

    glfwTerminate();
    return 0;
}


void framebuffer_size_callback(GLFWwindow* window, int width, int height) {
    glViewport(0, 0, width, height);
}

void initializeSlimes() {
    for (int i = 0; i < MAX_SLIMES; ++i) {
        slimes[i].ativo = false;
    }
    activeSlimesCount = 0;
}

void spawnSlime() {
    int newSlimeIndex = -1;
    if (activeSlimesCount < MAX_SLIMES) {
        newSlimeIndex = activeSlimesCount;
    } else {
        for (int i = 0; i < MAX_SLIMES; ++i) {
            if (!slimes[i].ativo) {
                newSlimeIndex = i;
                break;
            }
        }
    }

    if (newSlimeIndex == -1) { return; }

    Slime* newSlime = &slimes[newSlimeIndex];
    
    // ... (lógica de posição e cor como antes) ...
    int side = rand() % 4;
    float spawnX, spawnY;
    float slimeRadius = 20.0f;

    switch (side) {
        case 0: spawnX = (float)(rand() % SCR_WIDTH); spawnY = (float)SCR_HEIGHT + slimeRadius; break;
        case 1: spawnX = (float)(rand() % SCR_WIDTH); spawnY = 0.0f - slimeRadius; break;
        case 2: spawnX = 0.0f - slimeRadius; spawnY = (float)(rand() % SCR_HEIGHT); break;
        case 3: default: spawnX = (float)SCR_WIDTH + slimeRadius; spawnY = (float)(rand() % SCR_HEIGHT); break;
    }
    newSlime->x = spawnX;
    newSlime->y = spawnY;
    newSlime->z = 0.0f;
    newSlime->raio = slimeRadius;
    newSlime->cor[0] = (float)(rand() % 101) / 100.0f;
    newSlime->cor[1] = (float)(rand() % 101) / 100.0f;
    newSlime->cor[2] = (float)(rand() % 101) / 100.0f;
    if (newSlime->cor[0] < 0.2f && newSlime->cor[1] < 0.2f && newSlime->cor[2] < 0.2f) {
        newSlime->cor[rand() % 3] = 0.5f;
    }

    // INICIALIZAR VIDA
    newSlime->maxHealth = SLIME_MAX_HEALTH;
    newSlime->health = SLIME_MAX_HEALTH;
    newSlime->ativo = true;

    if (newSlimeIndex == activeSlimesCount) {
        activeSlimesCount++;
    }
}

void drawSlimes(unsigned int currentShaderProgram, unsigned int currentVAO, mat4 model_m_placeholder) {
    mat4 model_matrix; // Matriz de modelo local para cada objeto
    unsigned int modelLoc = glGetUniformLocation(currentShaderProgram, "model");
    unsigned int colorLoc = glGetUniformLocation(currentShaderProgram, "objectColor_Solid");

    vec3 health_bar_bg_color = {1.0f, 0.0f, 0.0f}; // Vermelho para fundo da barra
    vec3 health_bar_fg_color = {0.0f, 1.0f, 0.0f}; // Verde para vida atual

    glBindVertexArray(currentVAO);

    for (int i = 0; i < activeSlimesCount; ++i) {
        if (slimes[i].ativo) {
            Slime* currentSlime = &slimes[i];

            // 1. Desenhar o corpo do Slime (como antes)
            glUniform3fv(colorLoc, 1, currentSlime->cor);
            glm_mat4_identity(model_matrix);
            glm_translate(model_matrix, (vec3){currentSlime->x, currentSlime->y, currentSlime->z});
            glm_scale(model_matrix, (vec3){currentSlime->raio * 2.0f, currentSlime->raio * 2.0f, 1.0f});
            glUniformMatrix4fv(modelLoc, 1, GL_FALSE, (const GLfloat*)model_matrix);
            glDrawElements(GL_TRIANGLES, 6, GL_UNSIGNED_INT, 0);

            // --- Desenhar a Barra de Vida ---
            
            // Definições da Barra de Vida
            float bar_width = currentSlime->raio * 2.0f; // Mesma largura do slime
            float bar_height = 5.0f; // Altura fixa
            float bar_y_offset = currentSlime->raio + 5.0f; // Posição acima do slime

            // 2. Desenhar o fundo da barra de vida (vermelha, inteira)
            glUniform3fv(colorLoc, 1, health_bar_bg_color);
            glm_mat4_identity(model_matrix);
            glm_translate(model_matrix, (vec3){currentSlime->x, currentSlime->y + bar_y_offset, 0.0f});
            glm_scale(model_matrix, (vec3){bar_width, bar_height, 1.0f});
            glUniformMatrix4fv(modelLoc, 1, GL_FALSE, (const GLfloat*)model_matrix);
            glDrawElements(GL_TRIANGLES, 6, GL_UNSIGNED_INT, 0);

            // 3. Desenhar a frente da barra de vida (verde, proporcional à vida)
            float health_percentage = currentSlime->health / currentSlime->maxHealth;
            if (health_percentage < 0) health_percentage = 0; // Evitar escala negativa
            
            glUniform3fv(colorLoc, 1, health_bar_fg_color);
            glm_mat4_identity(model_matrix);
            // A translação precisa ser ajustada para que a barra verde diminua para a direita,
            // mantendo-se alinhada à esquerda da barra de fundo.
            // Movemos para a posição inicial (borda esquerda) e escalamos a partir dali.
            // Nosso VBO de quadrado unitário é centrado em (0,0).
            // Primeiro, transladamos para o centro da barra VERDE.
            float green_bar_width = bar_width * health_percentage;
            float green_bar_center_x = currentSlime->x - (bar_width / 2.0f) + (green_bar_width / 2.0f);
            
            glm_translate(model_matrix, (vec3){green_bar_center_x, currentSlime->y + bar_y_offset, 0.0f});
            glm_scale(model_matrix, (vec3){green_bar_width, bar_height, 1.0f});
            glUniformMatrix4fv(modelLoc, 1, GL_FALSE, (const GLfloat*)model_matrix);
            glDrawElements(GL_TRIANGLES, 6, GL_UNSIGNED_INT, 0);
        }
    }
}

void updateSlimes(float deltaTime, Player* p) {
    for (int i = 0; i < activeSlimesCount; ) {
        bool slime_was_deactivated = false;
        if (slimes[i].ativo) {
            Slime* currentSlime = &slimes[i];
            
            // ... (lógica de movimento do slime como antes) ...
            float dirX = p->position[0] - currentSlime->x;
            float dirY = p->position[1] - currentSlime->y;
            float length = sqrtf(dirX * dirX + dirY * dirY);
            if (length > 0) {
                dirX /= length;
                dirY /= length;
            }
            currentSlime->x += dirX * SLIME_SPEED * deltaTime;
            currentSlime->y += dirY * SLIME_SPEED * deltaTime;

            // --- Detecção de Colisão (Slime com Jogador) ---
            float playerLeft   = p->position[0] - p->size[0] / 2.0f;
            float playerRight  = p->position[0] + p->size[0] / 2.0f;
            float playerBottom = p->position[1] - p->size[1] / 2.0f;
            float playerTop    = p->position[1] + p->size[1] / 2.0f;

            float slimeLeft   = currentSlime->x - currentSlime->raio;
            float slimeRight  = currentSlime->x + currentSlime->raio;
            float slimeBottom = currentSlime->y - currentSlime->raio;
            float slimeTop    = currentSlime->y + currentSlime->raio;

            if (playerLeft < slimeRight && playerRight > slimeLeft &&
                playerBottom < slimeTop && playerTop > slimeBottom) {
                
                // APLICAR DANO AO JOGADOR
                p->health -= SLIME_COLLISION_DAMAGE;
                printf("JOGADOR SOFREU DANO! Vida restante: %.1f\n", p->health);

                // VERIFICAR SE O JOGADOR MORREU
                if (p->health <= 0) {
                    p->health = 0; // Evitar vida negativa
                    printf("JOGADOR DERROTADO! FIM DE JOGO.\n");
                    // MUDAR O ESTADO DO JOGO
                    currentGameState = GAME_STATE_GAMEOVER;
                }
                
                // DESATIVAR O SLIME para que ele não cause dano contínuo
                currentSlime->ativo = false;
                slime_was_deactivated = true;
            }
        }

        // --- Lógica de Compactação do Array ---
        // Se o slime foi desativado (por colisão com jogador OU projétil)
        if (!slimes[i].ativo || slime_was_deactivated) {
            activeSlimesCount--;
            slimes[i] = slimes[activeSlimesCount];
        } else {
            i++;
        }
    }
}

void processInput(GLFWwindow *window, Player *p, float deltaTime) {
    if (glfwGetKey(window, GLFW_KEY_ESCAPE) == GLFW_PRESS)
        glfwSetWindowShouldClose(window, true);

    if(currentGameState == GAME_STATE_PLAYING){
        // Movimentação
        float actual_speed = PLAYER_SPEED * deltaTime;
        if (glfwGetKey(window, GLFW_KEY_W) == GLFW_PRESS) p->position[1] += actual_speed;
        if (glfwGetKey(window, GLFW_KEY_S) == GLFW_PRESS) p->position[1] -= actual_speed;
        if (glfwGetKey(window, GLFW_KEY_A) == GLFW_PRESS) p->position[0] -= actual_speed;
        if (glfwGetKey(window, GLFW_KEY_D) == GLFW_PRESS) p->position[0] += actual_speed;

        // Rotação
        float rotation_change = PLAYER_ROTATION_SPEED * deltaTime;
        if (glfwGetKey(window, GLFW_KEY_LEFT) == GLFW_PRESS) {
            p->angle += rotation_change; // Aumentar ângulo (sentido anti-horário em matemática, pode ser horário visualmente dependendo da sua convenção)
        }
        if (glfwGetKey(window, GLFW_KEY_RIGHT) == GLFW_PRESS) {
            p->angle -= rotation_change; // Diminuir ângulo (sentido horário em matemática)
        }

        timeSinceLastShot += deltaTime; // Atualiza o contador do cooldown

        if (glfwGetKey(window, GLFW_KEY_SPACE) == GLFW_PRESS) {
            if (timeSinceLastShot >= FIRE_RATE) {
                fireProjectile(p);
                timeSinceLastShot = 0.0f; // Reseta o cooldown
            }
        }

        // Limites do Mapa (como antes)
        float half_width = p->size[0] * 0.7f / 2.0f; // Use o tamanho visual para limites se desejar
        float half_height = p->size[1] * 0.7f / 2.0f;
        if (p->position[0] - half_width < 0.0f) p->position[0] = half_width;
        if (p->position[0] + half_width > SCR_WIDTH) p->position[0] = SCR_WIDTH - half_width;
        if (p->position[1] - half_height < 0.0f) p->position[1] = half_height;
        if (p->position[1] + half_height > SCR_HEIGHT) p->position[1] = SCR_HEIGHT - half_height;
    }
    // Lógica de input para a tela de Game Over
    else if (currentGameState == GAME_STATE_GAMEOVER) {
        if (glfwGetKey(window, GLFW_KEY_R) == GLFW_PRESS) {
            resetGame();
        }
    }


}

unsigned int compile_shader(const char* source, GLenum type) {
    unsigned int shader = glCreateShader(type);
    glShaderSource(shader, 1, &source, NULL);
    glCompileShader(shader);
    int success;
    char infoLog[512];
    glGetShaderiv(shader, GL_COMPILE_STATUS, &success);
    if (!success) {
        glGetShaderInfoLog(shader, 512, NULL, infoLog);
        fprintf(stderr, "ERRO::SHADER::COMPILACAO_FALHOU (%s):\n%s\n",
                (type == GL_VERTEX_SHADER ? "VERTEX" : "FRAGMENT"), infoLog);
        glDeleteShader(shader);
        return 0;
    }
    return shader;
}

unsigned int create_shader_program(const char* vertex_source, const char* fragment_source) {
    unsigned int vertexShader = compile_shader(vertex_source, GL_VERTEX_SHADER);
    if (vertexShader == 0) {
        return 0;
    }
    unsigned int fragmentShader = compile_shader(fragment_source, GL_FRAGMENT_SHADER);
    if (fragmentShader == 0) {
        glDeleteShader(vertexShader);
        return 0;
    }

    unsigned int gameObjectShaderProgram = glCreateProgram();
    glAttachShader(gameObjectShaderProgram, vertexShader);
    glAttachShader(gameObjectShaderProgram, fragmentShader);
    glLinkProgram(gameObjectShaderProgram);

    int success;
    char infoLog[512];
    glGetProgramiv(gameObjectShaderProgram, GL_LINK_STATUS, &success);
    if (!success) {
        glGetProgramInfoLog(gameObjectShaderProgram, 512, NULL, infoLog);
        fprintf(stderr, "ERRO::SHADER::PROGRAMA::LINKAGEM_FALHOU\n%s\n", infoLog);
        glDeleteShader(vertexShader);
        glDeleteShader(fragmentShader);
        glDeleteProgram(gameObjectShaderProgram);
        return 0;
    }

    glDeleteShader(vertexShader);
    glDeleteShader(fragmentShader);
    return gameObjectShaderProgram;
}


void initializeProjectiles() {
    for (int i = 0; i < MAX_PROJECTILES; ++i) {
        projectiles[i].active = false;
    }
    // activeProjectilesCount = 0; // Se for usar um contador de ativos específico para projéteis
}

void drawProjectiles(unsigned int currentShaderProgram, unsigned int currentVAO, mat4 model_matrix_ref_placeholder) {
    mat4 model_proj_m;
    unsigned int modelLoc = glGetUniformLocation(currentShaderProgram, "model");
    unsigned int colorLoc = glGetUniformLocation(currentShaderProgram, "objectColor_Solid"); // NOME CORRIGIDO

    vec3 projectile_color = {1.0f, 1.0f, 0.0f};
    glUniform3fv(colorLoc, 1, projectile_color);

    glBindVertexArray(currentVAO);

    for (int i = 0; i < MAX_PROJECTILES; ++i) {
        if (projectiles[i].active) {
            Projectile* proj = &projectiles[i];

            glm_mat4_identity(model_proj_m);
            glm_translate(model_proj_m, (vec3){proj->position[0], proj->position[1], 0.0f});
            glm_scale(model_proj_m, (vec3){proj->size, proj->size, 1.0f});

            glUniformMatrix4fv(modelLoc, 1, GL_FALSE, (const GLfloat*)model_proj_m);
            glDrawElements(GL_TRIANGLES, 6, GL_UNSIGNED_INT, 0);
        }
    }
}


void fireProjectile(Player* p) {
    // Encontrar um projétil inativo para reutilizar
    int projectileIndex = -1;
    for (int i = 0; i < MAX_PROJECTILES; ++i) {
        if (!projectiles[i].active) {
            projectileIndex = i;
            break;
        }
    }

    if (projectileIndex == -1) {
        // printf("Sem projéteis disponíveis!\n");
        return; // Todos os projéteis estão ativos
    }

    Projectile* proj = &projectiles[projectileIndex];

    proj->active = true;
    proj->speed = PROJECTILE_SPEED;
    proj->size = PROJECTILE_SIZE;
    proj->angle = p->angle; // Projétil disparado na direção que o jogador está olhando
    proj->lifetime = PROJECTILE_LIFETIME;

    // Calcular a posição inicial do projétil
    // Começa na "ponta da arma". A arma já tem um offset do centro do jogador.
    // Queremos que o projétil apareça um pouco à frente da ponta da arma.

    // 1. Posição base (centro do jogador)
    vec2 start_pos;
    glm_vec2_copy(p->position, start_pos);

    // 2. Vetor da direção do jogador/arma
    float dirX = cosf(p->angle);
    float dirY = sinf(p->angle);

    // 3. Distância do centro do jogador até a "ponta" da arma (aproximado)
    //    weapon_offset_distance é o offset do *centro* da arma
    //    weapon_width é a largura total da arma
    //    Então, a ponta da arma está em weapon_offset_distance + weapon_width / 2
    //    ao longo da direção da arma.
    float weapon_actual_width = p->size[0] * 0.6f; // Mesma da renderização da arma
    float weapon_offset_from_player_center = p->size[0] * 0.4f; // Mesmo da renderização
    
    float gun_tip_offset = weapon_offset_from_player_center + (weapon_actual_width / 2.0f) + (proj->size / 2.0f); // Adiciona metade do tamanho do projétil para ele aparecer na ponta

    start_pos[0] += dirX * gun_tip_offset;
    start_pos[1] += dirY * gun_tip_offset;

    glm_vec2_copy(start_pos, proj->position);

    // Definir velocidade baseada no ângulo e velocidade escalar
    proj->velocity[0] = dirX * proj->speed;
    proj->velocity[1] = dirY * proj->speed;
}

void updateProjectiles(float deltaTime) {
    for (int i = 0; i < MAX_PROJECTILES; ++i) {
        if (projectiles[i].active) {
            Projectile* proj = &projectiles[i];

            // --- LÓGICA DE MOVIMENTO E LIFETIME (BLOCO FALTANTE) ---
            // 1. Mover o projétil usando sua velocidade e o tempo decorrido
            proj->position[0] += proj->velocity[0] * deltaTime;
            proj->position[1] += proj->velocity[1] * deltaTime;

            // 2. Reduzir o tempo de vida do projétil
            proj->lifetime -= deltaTime;
            if (proj->lifetime <= 0.0f) {
                proj->active = false;
                continue; // Pular para o próximo projétil, não precisa checar colisão
            }

            // 3. (Opcional) Desativar se sair da tela
            if (proj->position[0] < 0 || proj->position[0] > SCR_WIDTH ||
                proj->position[1] < 0 || proj->position[1] > SCR_HEIGHT) {
                proj->active = false;
                continue;
            }
            // --- FIM DO BLOCO FALTANTE ---


            // --- COLISÃO PROJÉTIL-SLIME ---
            // A lógica aqui permanece a mesma que na sugestão anterior
            for (int j = 0; j < activeSlimesCount; ++j) {
                if (slimes[j].ativo) {
                    Slime* currentSlime = &slimes[j];

                    // Detecção de colisão AABB
                    float projLeft = proj->position[0] - proj->size / 2.0f;
                    float projRight = proj->position[0] + proj->size / 2.0f;
                    float projBottom = proj->position[1] - proj->size / 2.0f;
                    float projTop = proj->position[1] + proj->size / 2.0f;

                    float slimeLeft   = currentSlime->x - currentSlime->raio;
                    float slimeRight  = currentSlime->x + currentSlime->raio;
                    float slimeBottom = currentSlime->y - currentSlime->raio;
                    float slimeTop    = currentSlime->y + currentSlime->raio;

                    if (projLeft < slimeRight && projRight > slimeLeft &&
                        projBottom < slimeTop && projTop > slimeBottom) {
                        
                        proj->active = false; // Desativar projétil
                        currentSlime->health -= PROJECTILE_DAMAGE; // Aplicar dano

                        printf("Slime %d sofreu dano! Vida restante: %.1f\n", j, currentSlime->health);

                        if (currentSlime->health <= 0) {
                            slimes[j].ativo = false; // Desativar slime se a vida acabar
                            printf("Slime %d foi derrotado!\n", j);
                        }
                        
                        break; // Sair do loop de slimes, pois o projétil já foi consumido
                    }
                }
            }
        }
    }
}

unsigned int loadTexture(const char *path) {
    unsigned int textureID;
    glGenTextures(1, &textureID);

    int width, height, nrComponents;
    // stbi_set_flip_vertically_on_load(true); // Descomente se a textura estiver de cabeça para baixo
    unsigned char *data = stbi_load(path, &width, &height, &nrComponents, 0);
   
   stbi_set_flip_vertically_on_load(true); 
    if (data) {
        GLenum format = GL_RGB; // Padrão para RGB
        if (nrComponents == 1)
            format = GL_RED;
        else if (nrComponents == 3)
            format = GL_RGB;
        else if (nrComponents == 4)
            format = GL_RGBA;
        else {
            fprintf(stderr, "Formato de imagem não suportado para %s (componentes: %d)\n", path, nrComponents);
            stbi_image_free(data);
            return 0;
        }

        glBindTexture(GL_TEXTURE_2D, textureID);
        glTexImage2D(GL_TEXTURE_2D, 0, format, width, height, 0, format, GL_UNSIGNED_BYTE, data);
        glGenerateMipmap(GL_TEXTURE_2D);

        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR_MIPMAP_LINEAR);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);

        stbi_image_free(data);
        printf("Textura carregada: %s (ID: %u, %dx%d, Comp: %d)\n", path, textureID, width, height, nrComponents);
    } else {
        fprintf(stderr, "Falha ao carregar textura: %s. Erro STB: %s\n", path, stbi_failure_reason());
        return 0;
    }
    return textureID;
}

void setupBackgroundGeometry() {
    float backgroundVerticesData[] = { // Renomeado para evitar conflito de nome
         (float)SCR_WIDTH, (float)SCR_HEIGHT, 0.0f,  1.0f, 1.0f,
         (float)SCR_WIDTH, 0.0f,              0.0f,  1.0f, 0.0f,
         0.0f,             0.0f,              0.0f,  0.0f, 0.0f,
         0.0f,             (float)SCR_HEIGHT, 0.0f,  0.0f, 1.0f
    };
    unsigned int backgroundIndicesData[] = { 0, 1, 3, 1, 2, 3 }; // Renomeado

    glGenVertexArrays(1, &backgroundVAO); // Usa a variável global backgroundVAO
    glGenBuffers(1, &backgroundVBO);     // Usa a variável global backgroundVBO
    glGenBuffers(1, &backgroundEBO);     // Usa a variável global backgroundEBO

    glBindVertexArray(backgroundVAO);
    glBindBuffer(GL_ARRAY_BUFFER, backgroundVBO);
    glBufferData(GL_ARRAY_BUFFER, sizeof(backgroundVerticesData), backgroundVerticesData, GL_STATIC_DRAW);
    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, backgroundEBO);
    glBufferData(GL_ELEMENT_ARRAY_BUFFER, sizeof(backgroundIndicesData), backgroundIndicesData, GL_STATIC_DRAW);

    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 5 * sizeof(float), (void*)0);
    glEnableVertexAttribArray(0);
    glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, 5 * sizeof(float), (void*)(3 * sizeof(float)));
    glEnableVertexAttribArray(1);
    glBindVertexArray(0);
}

void resetGame() {
    printf("Reiniciando o jogo...\n");

    // Resetar jogador
    player.position[0] = SCR_WIDTH / 2.0f;
    player.position[1] = SCR_HEIGHT / 2.0f;
    player.angle = 0.0f;
    player.health = PLAYER_MAX_HEALTH;

    // Resetar slimes e projéteis
    initializeSlimes();
    initializeProjectiles();

    // Resetar contadores de tempo
    timeSinceLastSpawn = 0.0f; // Esta é uma variável global, precisa ser resetada aqui
    timeSinceLastShot = 0.0f;  // Também global

    // Mudar o estado de volta para JOGANDO
    currentGameState = GAME_STATE_PLAYING;
}
