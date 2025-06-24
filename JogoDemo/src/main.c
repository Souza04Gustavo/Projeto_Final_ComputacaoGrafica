// PARA COMPILAR:
// gcc src/main.c src/glad.c -o MeuJogo -Iinclude -lglfw -lGL -lm -ldl -pthread

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
#define GIANT_SLIME_CHANCE 5 // Chance de 1 em 5 de um slime ser gigante
#define GIANT_SLIME_SIZE_MULTIPLIER 1.8f // 80% maior que o normal
#define GIANT_SLIME_HEALTH_MULTIPLIER 3.0f // 3x mais vida
#define MAX_PROJECTILES 50
#define PROJECTILE_SPEED 500.0f
#define PROJECTILE_SIZE 10.0f
#define PROJECTILE_LIFETIME 2.0f // Segundos
#define FIRE_RATE 0.25f // Segundos entre disparos (4 tiros por segundo)
#define PROJECTILE_DAMAGE 25.0f // Cada tiro tira 25 de vida
#define PLAYER_MAX_HEALTH 100.0f
#define SLIME_COLLISION_DAMAGE 25.0f // Dano que o slime causa ao colidir
#define MAX_PARTICLES 500       // Máximo de partículas na tela ao mesmo tempo
#define PARTICLE_LIFETIME 1.0f  // Segundos que uma partícula vive
#define PARTICLE_SPEED 100.0f   // Velocidade base das partículas

int SLIME_SPEED = 150.0f;  // Velocidade dos slimes
int SLIME_MAX_HEALTH = 100.0f; // Slimes começam com 100 de vida

// Timers e estados para a tela de vitória
bool g_ShowingVictoryScreen = false;
float g_VictoryScreenTimer = 10.0f; // 10 segundos

mat4 g_ProjectionMatrix;

const float PLAYER_SPEED = 250.0f; // Pixels por segundo (ajustei um pouco para cima)
const float PLAYER_ROTATION_SPEED = 3.0f; // Radianos por segundo (ajuste conforme necessário)
const char *caminho_background = "shaders/chao/fundo_ia1.png";
const char *caminho_player = "shaders/player/handgun/idle/survivor-idle_handgun_1.png";
const char *caminho_game_over = "shaders/interfaces/gameover.png";
const char *caminho_menu_title = "assets/menu_title.png";
const unsigned int SCR_WIDTH = 800;
const unsigned int SCR_HEIGHT = 600;
unsigned int g_CurrentScreenWidth = SCR_WIDTH;
unsigned int g_CurrentScreenHeight = SCR_HEIGHT;
float g_ScaleFactor = 1.0f; // Fator de escala para os objetos do jogo
int g_CurrentPhase = 1;
int g_KillsCount = 0;
int g_KillsAtPhaseStart = 0;
const int KILLS_FOR_PHASE_2 = 15;
const int KILLS_FOR_PHASE_3 = 25; // Mortes TOTAIS, não 25 a mais
const int KILLS_FOR_VICTORY = 40; // Mortes TOTAIS para vencer


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
    float lifetime;
} Projectile;


typedef enum {
    GAME_STATE_MENU,
    GAME_STATE_PLAYING,
    GAME_STATE_GAMEOVER,
    GAME_STATE_VICTORY
} GameState;


typedef enum {
    DIFFICULTY_EASY,
    DIFFICULTY_NORMAL,
    DIFFICULTY_HARD
} Difficulty;


typedef struct {
    float x, y, width, height; // Posição (centro) e dimensões
    unsigned int texture_normal;
    unsigned int texture_hover;
    bool is_hovered;
} MenuButton;


typedef struct {
    vec2 position;
    vec2 velocity;
    vec4 color;
    float life;
    bool active;
} Particle;


// Assinaturas das funcoes
void framebuffer_size_callback(GLFWwindow* window, int width, int height);
void processInput(GLFWwindow *window, Player *player, float deltaTime);
unsigned int compile_shader(const char* source, GLenum type);
unsigned int create_shader_program(const char* vertex_source, const char* fragment_source);
void initializeSlimes();
void spawnSlime();
void updateSlimes(float deltaTime, Player* player);
void drawSlimes(unsigned int gameObjectShaderProgram, unsigned int gameObjectVAO, mat4 model_m_ref);
void initializeProjectiles();
void fireProjectile(Player* p);
void updateProjectiles(float deltaTime);
void drawProjectiles(unsigned int currentShaderProgram, unsigned int currentVAO, mat4 model_matrix_ref_placeholder);
unsigned int loadTexture(const char *path);
void setupBackgroundGeometry();
void resetGame();
void drawText(unsigned int shaderProgram, unsigned int vao, const char* text, float x, float y, float scale);
void drawCharacter(unsigned int shaderProgram, unsigned int vao, char character, float x, float y, float scale);
void mouse_pos_callback(GLFWwindow* window, double xpos, double ypos);
void mouse_button_callback(GLFWwindow* window, int button, int action, int mods);
void checkMenuButtonHovers();
void initializeParticles();
void spawnParticleExplosion(vec2 position, vec3 baseColor, int count);
void updateParticles(float deltaTime);
void drawParticles(unsigned int shaderProgram, unsigned int vao);


Player player;
Slime slimes[MAX_SLIMES];
Projectile projectiles[MAX_PROJECTILES];
GameState currentGameState = GAME_STATE_MENU;
Difficulty selectedDifficulty = DIFFICULTY_NORMAL; // Dificuldade padrão
MenuButton startButton;
MenuButton difficultyButton_Easy, difficultyButton_Normal, difficultyButton_Hard;
Particle particles[MAX_PARTICLES];

unsigned int playerTextureID; // ID da textura do jogador
int activeProjectilesCount = 0; // Se você não for compactar o array, este não será usado assim
int activeSlimesCount = 0; // Contador de slimes atualmente ativos
float timeSinceLastShot = 0.0f;
float timeSinceLastSpawn = 0.0f;
float spawnInterval = 3.0f; // Intervalo de spawn de slimes em segundos

double mouseX, mouseY;
bool key_up_pressed_last_frame = false;
bool key_down_pressed_last_frame = false;

// IDs das texturas
unsigned int backgroundTextureID_Phase1;
unsigned int backgroundTextureID_Phase2;
unsigned int backgroundTextureID_Phase3;
unsigned int victoryTextureID;
unsigned int slimeFaceTextureID;
unsigned int backgroundTextureID;
unsigned int menuBackgroundTextureID;
unsigned int gameOverTextureID;
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
    "in vec2 TexCoord_Object;\n"
    "uniform sampler2D objectTexture_Sampler;\n"
    "uniform bool useTexture;\n"
    
    "uniform vec4 objectColor_Solid;\n"
    
    "void main()\n"
    "{\n"
    "   if (useTexture) {\n"
    "       vec4 texColor = texture(objectTexture_Sampler, TexCoord_Object);\n"
    "       if(texColor.a < 0.1) discard;\n"
    "       FragColor = texColor;\n"
    "   } else {\n"
    "       FragColor = objectColor_Solid;\n"
    "   }\n"
    "}\0";

const char *backgroundVertexShaderSource_global = "#version 330 core\n"
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

const char *backgroundFragmentShaderSource_global = "#version 330 core\n"
    "out vec4 FragColor;\n"
    "in vec2 TexCoord;\n"
    "uniform sampler2D backgroundTexture;\n"
    "void main()\n"
    "{\n"
    "   FragColor = texture(backgroundTexture, TexCoord);\n"
    "}\0";



int main() {
    srand(time(NULL));

    // 1. INICIALIZAÇÃO DO GLFW
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

    // 2. OBTER INFORMAÇÕES DO MONITOR PARA TELA CHEIA
    GLFWmonitor* primaryMonitor = glfwGetPrimaryMonitor();
    const GLFWvidmode* mode = glfwGetVideoMode(primaryMonitor);
    glfwWindowHint(GLFW_RED_BITS, mode->redBits);
    glfwWindowHint(GLFW_GREEN_BITS, mode->greenBits);
    glfwWindowHint(GLFW_BLUE_BITS, mode->blueBits);
    glfwWindowHint(GLFW_REFRESH_RATE, mode->refreshRate);

    // 3. CRIAR A JANELA
    GLFWwindow* window = glfwCreateWindow(mode->width, mode->height, "Jogo 2D", primaryMonitor, NULL);
    if (window == NULL) {
        fprintf(stderr, "Falha ao criar a janela GLFW em tela cheia\n");
        glfwTerminate();
        return -1;
    }

    // 4. CONFIGURAR CONTEXTO E CARREGAR GLAD
    glfwMakeContextCurrent(window);
    if (!gladLoadGLLoader((GLADloadproc)glfwGetProcAddress)) {
        fprintf(stderr, "Falha ao inicializar GLAD\n");
        glfwTerminate();
        return -1;
    }
    
    initializeParticles();

    // 5. CONFIGURAR CALLBACKS E ESTADO INICIAL DO OPENGL
    glfwSetInputMode(window, GLFW_CURSOR, GLFW_CURSOR_NORMAL);
    glfwSetFramebufferSizeCallback(window, framebuffer_size_callback);
    glfwSetCursorPosCallback(window, mouse_pos_callback);
    glfwSetMouseButtonCallback(window, mouse_button_callback);
    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA); // Modo de blend padrão
    
    // 6. CRIAR SHADER E VAO ÚNICOS
    unsigned int gameObjectShaderProgram = create_shader_program(vertexShaderSource, fragmentShaderSource);
    if (gameObjectShaderProgram == 0) { glfwTerminate(); return -1; }

    unsigned int gameObjectVAO;
    unsigned int gameObjectVBO, gameObjectEBO;
    float gameObjectVertices[] = {
         0.5f,  0.5f, 0.0f,  1.0f, 1.0f,
         0.5f, -0.5f, 0.0f,  1.0f, 0.0f,
        -0.5f, -0.5f, 0.0f,  0.0f, 0.0f,
        -0.5f,  0.5f, 0.0f,  0.0f, 1.0f
    };
    unsigned int gameObjectIndices[] = { 0, 1, 3, 1, 2, 3 };
    glGenVertexArrays(1, &gameObjectVAO);
    glGenBuffers(1, &gameObjectVBO);
    glGenBuffers(1, &gameObjectEBO);
    glBindVertexArray(gameObjectVAO);
    glBindBuffer(GL_ARRAY_BUFFER, gameObjectVBO);
    glBufferData(GL_ARRAY_BUFFER, sizeof(gameObjectVertices), gameObjectVertices, GL_STATIC_DRAW);
    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, gameObjectEBO);
    glBufferData(GL_ELEMENT_ARRAY_BUFFER, sizeof(gameObjectIndices), gameObjectIndices, GL_STATIC_DRAW);
    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 5 * sizeof(float), (void*)0);
    glEnableVertexAttribArray(0);
    glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, 5 * sizeof(float), (void*)(3 * sizeof(float)));
    glEnableVertexAttribArray(1);
    glBindVertexArray(0);

    // 7. CARREGAR TODAS AS TEXTURAS (removido o glowTextureID)
    playerTextureID = loadTexture(caminho_player);
    backgroundTextureID = loadTexture(caminho_background);
    gameOverTextureID = loadTexture(caminho_game_over);
    menuBackgroundTextureID = loadTexture("shaders/interfaces/background_menu.png");
    startButton.texture_normal = loadTexture("shaders/interfaces/botao_final.png");
    startButton.texture_hover = loadTexture("shaders/interfaces/botao_final_apagado.png");
    slimeFaceTextureID = loadTexture("shaders/slimes/slime_face.png");
    difficultyButton_Easy.texture_normal = loadTexture("shaders/interfaces/facil.png");
    difficultyButton_Easy.texture_hover = loadTexture("shaders/interfaces/facil_selecionado.png");
    difficultyButton_Normal.texture_normal = loadTexture("shaders/interfaces/medio.png");
    difficultyButton_Normal.texture_hover = loadTexture("shaders/interfaces/medio_selecionado.png");
    difficultyButton_Hard.texture_normal = loadTexture("shaders/interfaces/dificil.png");
    difficultyButton_Hard.texture_hover = loadTexture("shaders/interfaces/dificil_selecionado.png");
    backgroundTextureID_Phase1 = loadTexture(caminho_background);
    backgroundTextureID_Phase2 = loadTexture("shaders/chao/fundo_fase2.png");
    backgroundTextureID_Phase3 = loadTexture("shaders/chao/fundo_fase3.png");
    victoryTextureID = loadTexture("shaders/interfaces/tela_vitoria.png");

    // 8. CONFIGURAR ESTADO INICIAL DO JOGO
    startButton.width = 300.0f; startButton.height = 140.0f;
    float diff_btn_width = 320.0f, diff_btn_height = 140.0f;
    difficultyButton_Easy.width = difficultyButton_Normal.width = difficultyButton_Hard.width = diff_btn_width;
    difficultyButton_Easy.height = difficultyButton_Normal.height = difficultyButton_Hard.height = diff_btn_height;
    
    mat4 view_m;
    glm_mat4_identity(view_m);
    framebuffer_size_callback(window, mode->width, mode->height);

    float lastFrameTime = 0.0f, deltaTime;

    // Localizações de Uniforms
    unsigned int modelLoc = glGetUniformLocation(gameObjectShaderProgram, "model");
    unsigned int viewLoc  = glGetUniformLocation(gameObjectShaderProgram, "view");
    unsigned int projLoc  = glGetUniformLocation(gameObjectShaderProgram, "projection");
    unsigned int colorSolidLoc = glGetUniformLocation(gameObjectShaderProgram, "objectColor_Solid");
    unsigned int useTextureLoc = glGetUniformLocation(gameObjectShaderProgram, "useTexture");
    unsigned int objectTextureSamplerLoc = glGetUniformLocation(gameObjectShaderProgram, "objectTexture_Sampler");

    // ===== LOOP PRINCIPAL SIMPLIFICADO =====
    while (!glfwWindowShouldClose(window)) {
        float currentFrameTime = (float)glfwGetTime();
        deltaTime = currentFrameTime - lastFrameTime;
        lastFrameTime = currentFrameTime;

        processInput(window, &player, deltaTime);
        glfwPollEvents();
        
        // Limpa a tela principal (framebuffer 0)
        glClearColor(0.1f, 0.1f, 0.15f, 1.0f);
        glClear(GL_COLOR_BUFFER_BIT);

        // Prepara o shader principal uma vez por frame
        glUseProgram(gameObjectShaderProgram);
        glUniformMatrix4fv(viewLoc, 1, GL_FALSE, (const GLfloat*)view_m);
        glUniformMatrix4fv(projLoc, 1, GL_FALSE, (const GLfloat*)g_ProjectionMatrix);
        glBindVertexArray(gameObjectVAO);
        glActiveTexture(GL_TEXTURE0);
        glUniform1i(objectTextureSamplerLoc, 0);

        if(currentGameState == GAME_STATE_PLAYING){
            // Lógica de atualização
            timeSinceLastSpawn += deltaTime;
            if (timeSinceLastSpawn >= spawnInterval) {
                spawnSlime();
                timeSinceLastSpawn = 0.0f;
            }
            updateSlimes(deltaTime, &player);
            updateProjectiles(deltaTime);
            updateParticles(deltaTime);

            // RENDERIZAÇÃO DIRETA NA TELA
            // Desenhar fundo
            glUniform1i(useTextureLoc, true);
            unsigned int currentBackgroundID;
        if (g_CurrentPhase == 1) {
            currentBackgroundID = backgroundTextureID_Phase1;
        } else if (g_CurrentPhase == 2) {
            currentBackgroundID = backgroundTextureID_Phase2;
        } else { // Fase 3
            currentBackgroundID = backgroundTextureID_Phase3;
        }
        glBindTexture(GL_TEXTURE_2D, currentBackgroundID);
            mat4 model_background;
            glm_mat4_identity(model_background);
            glm_translate(model_background, (vec3){g_CurrentScreenWidth / 2.0f, g_CurrentScreenHeight / 2.0f, 0.0f});
            glm_scale(model_background, (vec3){g_CurrentScreenWidth, g_CurrentScreenHeight, 1.0f});
            glUniformMatrix4fv(modelLoc, 1, GL_FALSE, (const GLfloat*)model_background);
            glDrawElements(GL_TRIANGLES, 6, GL_UNSIGNED_INT, 0);

            // Desenhar jogador
            glBindTexture(GL_TEXTURE_2D, playerTextureID);
            mat4 model_player;
            glm_mat4_identity(model_player);
            glm_translate(model_player, (vec3){player.position[0], player.position[1], 0.0f});
            glm_rotate_z(model_player, player.angle, model_player);
            glm_scale(model_player, (vec3){player.size[0], player.size[1], 1.0f});
            glUniformMatrix4fv(modelLoc, 1, GL_FALSE, (const GLfloat*)model_player);
            glDrawElements(GL_TRIANGLES, 6, GL_UNSIGNED_INT, 0);

            // Desenhar objetos com cor
            glUniform1i(useTextureLoc, false);
            drawProjectiles(gameObjectShaderProgram, gameObjectVAO, model_player);
            drawSlimes(gameObjectShaderProgram, gameObjectVAO, model_player);
            drawParticles(gameObjectShaderProgram, gameObjectVAO);

            // Desenhar HUD
            float hud_bar_width = 200.0f * g_ScaleFactor;
            float hud_bar_height = 20.0f * g_ScaleFactor;
            float hud_bar_padding = 10.0f * g_ScaleFactor;
            float hud_bar_x = g_CurrentScreenWidth - hud_bar_width - hud_bar_padding;
            float hud_bar_y = g_CurrentScreenHeight - hud_bar_height - hud_bar_padding;

            glUniform4f(colorSolidLoc, 1.0f, 0.0f, 0.0f, 1.0f); // Vermelho opaco
            mat4 model_hud_bg;
            glm_mat4_identity(model_hud_bg);
            glm_translate(model_hud_bg, (vec3){hud_bar_x + hud_bar_width / 2.0f, hud_bar_y + hud_bar_height / 2.0f, 0.0f});
            glm_scale(model_hud_bg, (vec3){hud_bar_width, hud_bar_height, 1.0f});
            glUniformMatrix4fv(modelLoc, 1, GL_FALSE, (const GLfloat*)model_hud_bg);
            glDrawElements(GL_TRIANGLES, 6, GL_UNSIGNED_INT, 0);

            float health_percentage = player.health / player.maxHealth;
            if (health_percentage < 0) health_percentage = 0;
            float green_bar_width = hud_bar_width * health_percentage;
            if (green_bar_width > 0) {
                glUniform4f(colorSolidLoc, 0.0f, 1.0f, 0.0f, 1.0f); // Verde opaco
                mat4 model_hud_fg;
                glm_mat4_identity(model_hud_fg);
                float green_bar_center_x = hud_bar_x + (green_bar_width / 2.0f);
                glm_translate(model_hud_fg, (vec3){green_bar_center_x, hud_bar_y + hud_bar_height / 2.0f, 0.0f});
                glm_scale(model_hud_fg, (vec3){green_bar_width, hud_bar_height, 1.0f});
                glUniformMatrix4fv(modelLoc, 1, GL_FALSE, (const GLfloat*)model_hud_fg);
                glDrawElements(GL_TRIANGLES, 6, GL_UNSIGNED_INT, 0);
            }

            // ===== DESENHAR HUD DE PROGRESSO DE FASE (CANTO SUPERIOR ESQUERDO) =====

            // Posição da barra no canto superior esquerdo
            float progress_hud_x = hud_bar_padding;
            float progress_hud_y = g_CurrentScreenHeight - hud_bar_height - hud_bar_padding;

            // 1. Desenhar fundo da barra (Azul Escuro)
            glUniform4f(colorSolidLoc, 0.0f, 0.0f, 0.5f, 1.0f); // Cor azul escuro
            mat4 model_progress_bg;
            glm_mat4_identity(model_progress_bg);
            // Calcula o centro da barra de fundo
            glm_translate(model_progress_bg, (vec3){progress_hud_x + hud_bar_width / 2.0f, progress_hud_y + hud_bar_height / 2.0f, 0.0f});
            glm_scale(model_progress_bg, (vec3){hud_bar_width, hud_bar_height, 1.0f});
            glUniformMatrix4fv(modelLoc, 1, GL_FALSE, (const GLfloat*)model_progress_bg);
            glDrawElements(GL_TRIANGLES, 6, GL_UNSIGNED_INT, 0);

            // 2. Calcular a porcentagem de preenchimento da barra
            int kills_target_for_phase = 0;
            if (g_CurrentPhase == 1) {
                kills_target_for_phase = KILLS_FOR_PHASE_2;
            } else if (g_CurrentPhase == 2) {
                kills_target_for_phase = KILLS_FOR_PHASE_3;
            } else { // Fase 3
                kills_target_for_phase = KILLS_FOR_VICTORY;
            }

            int kills_needed_for_this_phase = kills_target_for_phase - g_KillsAtPhaseStart;
            int kills_made_in_this_phase = g_KillsCount - g_KillsAtPhaseStart;

            float progress_percentage = 0.0f;
            if (kills_needed_for_this_phase > 0) {
                progress_percentage = (float)kills_made_in_this_phase / (float)kills_needed_for_this_phase;
            }
            if (progress_percentage > 1.0f) progress_percentage = 1.0f; // Garante que não passe de 100%
            if (progress_percentage < 0.0f) progress_percentage = 0.0f;

            // 3. Desenhar barra de progresso (Azul Claro)
            float blue_bar_width = hud_bar_width * progress_percentage;
            if (blue_bar_width > 0) {
                glUniform4f(colorSolidLoc, 0.2f, 0.5f, 1.0f, 1.0f); // Cor azul claro
                mat4 model_progress_fg;
                glm_mat4_identity(model_progress_fg);
                // O centro da barra de progresso se move conforme ela cresce
                float blue_bar_center_x = progress_hud_x + (blue_bar_width / 2.0f);
                glm_translate(model_progress_fg, (vec3){blue_bar_center_x, progress_hud_y + hud_bar_height / 2.0f, 0.0f});
                glm_scale(model_progress_fg, (vec3){blue_bar_width, hud_bar_height, 1.0f});
                glUniformMatrix4fv(modelLoc, 1, GL_FALSE, (const GLfloat*)model_progress_fg);
                glDrawElements(GL_TRIANGLES, 6, GL_UNSIGNED_INT, 0);
            }


        }
        else if (currentGameState == GAME_STATE_GAMEOVER) {
        glUniform1i(useTextureLoc, true);
        glBindTexture(GL_TEXTURE_2D, backgroundTextureID);
        mat4 model_bg;
        glm_mat4_identity(model_bg);
        glm_translate(model_bg, (vec3){g_CurrentScreenWidth / 2.0f, g_CurrentScreenHeight / 2.0f, 0.0f});
        glm_scale(model_bg, (vec3){g_CurrentScreenWidth, g_CurrentScreenHeight, 1.0f});
        glUniformMatrix4fv(modelLoc, 1, GL_FALSE, (const GLfloat*)model_bg);
        glDrawElements(GL_TRIANGLES, 6, GL_UNSIGNED_INT, 0);

        glBindTexture(GL_TEXTURE_2D, gameOverTextureID);
        float gameOverImageWidth = 600.0f * g_ScaleFactor;
        float gameOverImageHeight = 300.0f * g_ScaleFactor;
        mat4 model_gameover;
        glm_mat4_identity(model_gameover);
        glm_translate(model_gameover, (vec3){g_CurrentScreenWidth / 2.0f, g_CurrentScreenHeight / 2.0f, 0.0f});
        glm_scale(model_gameover, (vec3){gameOverImageWidth, gameOverImageHeight, 1.0f});
        glUniformMatrix4fv(modelLoc, 1, GL_FALSE, (const GLfloat*)model_gameover);
        glDrawElements(GL_TRIANGLES, 6, GL_UNSIGNED_INT, 0);
        }
        else if (currentGameState == GAME_STATE_MENU) {
            checkMenuButtonHovers();
            glUniform1i(useTextureLoc, true);
            
            glBindTexture(GL_TEXTURE_2D, menuBackgroundTextureID);
            mat4 model_menu_bg;
            glm_mat4_identity(model_menu_bg);
            glm_translate(model_menu_bg, (vec3){g_CurrentScreenWidth / 2.0f, g_CurrentScreenHeight / 2.0f, 0.0f});
            glm_scale(model_menu_bg, (vec3){g_CurrentScreenWidth, g_CurrentScreenHeight, 1.0f});
            glUniformMatrix4fv(modelLoc, 1, GL_FALSE, (const GLfloat*)model_menu_bg);
            glDrawElements(GL_TRIANGLES, 6, GL_UNSIGNED_INT, 0);
            
            mat4 model_button;
            float btn_width_scaled = startButton.width * g_ScaleFactor;
            float btn_height_scaled = startButton.height * g_ScaleFactor;
            
            glBindTexture(GL_TEXTURE_2D, startButton.is_hovered ? startButton.texture_hover : startButton.texture_normal);
            glm_mat4_identity(model_button);
            glm_translate(model_button, (vec3){startButton.x, startButton.y, 0.0f});
            glm_scale(model_button, (vec3){btn_width_scaled, btn_height_scaled, 1.0f});
            glUniformMatrix4fv(modelLoc, 1, GL_FALSE, (const GLfloat*)model_button);
            glDrawElements(GL_TRIANGLES, 6, GL_UNSIGNED_INT, 0);

            float diff_btn_width_scaled = difficultyButton_Normal.width * g_ScaleFactor;
            float diff_btn_height_scaled = difficultyButton_Normal.height * g_ScaleFactor;
            MenuButton* currentDifficultyButton = (selectedDifficulty == DIFFICULTY_EASY) ? &difficultyButton_Easy : (selectedDifficulty == DIFFICULTY_NORMAL) ? &difficultyButton_Normal : &difficultyButton_Hard;
            glBindTexture(GL_TEXTURE_2D, currentDifficultyButton->is_hovered ? currentDifficultyButton->texture_hover : currentDifficultyButton->texture_normal);
            glm_mat4_identity(model_button);
            glm_translate(model_button, (vec3){currentDifficultyButton->x, currentDifficultyButton->y, 0.0f});
            glm_scale(model_button, (vec3){diff_btn_width_scaled, diff_btn_height_scaled, 1.0f});
            glUniformMatrix4fv(modelLoc, 1, GL_FALSE, (const GLfloat*)model_button);
            glDrawElements(GL_TRIANGLES, 6, GL_UNSIGNED_INT, 0);
        }
        else if (currentGameState == GAME_STATE_VICTORY) {
        // --- ATUALIZAR O TIMER DA TELA DE VITÓRIA ---
        g_VictoryScreenTimer -= deltaTime;
        if (g_VictoryScreenTimer <= 0) {
            currentGameState = GAME_STATE_MENU; // Volta para o menu
        }

        // --- DESENHAR A TELA DE VITÓRIA ---
        glUniform1i(useTextureLoc, true);
        glBindTexture(GL_TEXTURE_2D, victoryTextureID);

        // Centraliza a imagem na tela
        mat4 model_victory;
        glm_mat4_identity(model_victory);
        glm_translate(model_victory, (vec3){g_CurrentScreenWidth / 2.0f, g_CurrentScreenHeight / 2.0f, 0.0f});
        // Escala para o tamanho da tela inteira ou um tamanho fixo
        glm_scale(model_victory, (vec3){g_CurrentScreenWidth, g_CurrentScreenHeight, 1.0f});
        glUniformMatrix4fv(modelLoc, 1, GL_FALSE, (const GLfloat*)model_victory);
        glDrawElements(GL_TRIANGLES, 6, GL_UNSIGNED_INT, 0);
    }
    

        glfwSwapBuffers(window);
    }

    // ===== LIMPEZA FINAL SIMPLIFICADA =====
    glDeleteVertexArrays(1, &gameObjectVAO);
    glDeleteBuffers(1, &gameObjectVBO);
    glDeleteBuffers(1, &gameObjectEBO);
    glDeleteProgram(gameObjectShaderProgram);

    // Deleta apenas as texturas que foram carregadas
    glDeleteTextures(1, &playerTextureID);
    glDeleteTextures(1, &backgroundTextureID);
    glDeleteTextures(1, &gameOverTextureID);
    glDeleteTextures(1, &menuBackgroundTextureID);
    glDeleteTextures(1, &startButton.texture_normal);
    glDeleteTextures(1, &startButton.texture_hover);

    glfwTerminate();
    return 0;
}


void framebuffer_size_callback(GLFWwindow* window, int width, int height) {
    if (width == 0 || height == 0) return;

    g_CurrentScreenWidth = width;
    g_CurrentScreenHeight = height;

    glViewport(0, 0, width, height);
    glm_ortho(0.0f, (float)g_CurrentScreenWidth, 0.0f, (float)g_CurrentScreenHeight, -1.0f, 1.0f, g_ProjectionMatrix);
    
    // ===== CORREÇÃO: CALCULAR O FATOR DE ESCALA GLOBAL =====
    // Usamos a altura como base para manter a proporção correta dos sprites.
    g_ScaleFactor = (float)height / (float)SCR_HEIGHT;

    startButton.x = (float)g_CurrentScreenWidth / 2.0f;
    startButton.y = (float)g_CurrentScreenHeight / 2.0f + 50.0f; // Manter offset em pixels
    float diff_btn_x = (float)g_CurrentScreenWidth / 2.0f;
    float diff_btn_y = (float)g_CurrentScreenHeight / 2.0f - 200.0f;
    difficultyButton_Easy.x = difficultyButton_Normal.x = difficultyButton_Hard.x = diff_btn_x;
    difficultyButton_Easy.y = difficultyButton_Normal.y = difficultyButton_Hard.y = diff_btn_y;
}


// Funcoes para SLIMES
void initializeSlimes() {
    for (int i = 0; i < MAX_SLIMES; ++i) {
        slimes[i].ativo = false;
    }
    activeSlimesCount = 0;
}

void spawnSlime() {
    if (activeSlimesCount >= MAX_SLIMES) {
        return;
    }

    Slime* newSlime = &slimes[activeSlimesCount];
    
    // Decide aleatoriamente se o slime será gigante
    bool isGiant = (rand() % GIANT_SLIME_CHANCE == 0);

    if (isGiant) {
        // Propriedades do Slime Gigante
        newSlime->raio = (20.0f * g_ScaleFactor) * GIANT_SLIME_SIZE_MULTIPLIER;
        newSlime->maxHealth = SLIME_MAX_HEALTH * GIANT_SLIME_HEALTH_MULTIPLIER;
        newSlime->health = newSlime->maxHealth;
    } else {
        // Propriedades do Slime Normal
        newSlime->raio = 20.0f * g_ScaleFactor;
        newSlime->maxHealth = SLIME_MAX_HEALTH;
        newSlime->health = SLIME_MAX_HEALTH;
    }

    // Lógica de spawn fora da tela (comum para ambos os tipos)
    int side = rand() % 4;
    float spawnX, spawnY;
    switch (side) {
        case 0: spawnX = (float)(rand() % g_CurrentScreenWidth); spawnY = (float)g_CurrentScreenHeight + newSlime->raio; break;
        case 1: spawnX = (float)(rand() % g_CurrentScreenWidth); spawnY = 0.0f - newSlime->raio; break;
        case 2: spawnX = 0.0f - newSlime->raio; spawnY = (float)(rand() % g_CurrentScreenHeight); break;
        default: spawnX = (float)g_CurrentScreenWidth + newSlime->raio; spawnY = (float)(rand() % g_CurrentScreenHeight); break;
    }
    
    newSlime->x = spawnX;
    newSlime->y = spawnY;
    newSlime->z = 0.0f;
    newSlime->cor[0] = (float)(rand() % 101) / 100.0f;
    newSlime->cor[1] = (float)(rand() % 101) / 100.0f;
    newSlime->cor[2] = (float)(rand() % 101) / 100.0f;
    newSlime->ativo = true;

    activeSlimesCount++;
}

void drawSlimes(unsigned int currentShaderProgram, unsigned int currentVAO, mat4 model_m_ref) {
    mat4 model_matrix;
    unsigned int modelLoc = glGetUniformLocation(currentShaderProgram, "model");
    unsigned int colorLoc = glGetUniformLocation(currentShaderProgram, "objectColor_Solid");
    unsigned int useTextureLoc = glGetUniformLocation(currentShaderProgram, "useTexture");

    vec4 health_bar_bg_color = {1.0f, 0.0f, 0.0f, 1.0f};
    vec4 health_bar_fg_color = {0.0f, 1.0f, 0.0f, 1.0f};

    glBindVertexArray(currentVAO);

    for (int i = 0; i < activeSlimesCount; ++i) {
        Slime* currentSlime = &slimes[i];
        
        // ===== PASSO 1: DESENHAR O CORPO COLORIDO DO SLIME =====
        glUniform1i(useTextureLoc, false); // Usaremos cor sólida para o corpo
        vec4 slimeSolidColor = { currentSlime->cor[0], currentSlime->cor[1], currentSlime->cor[2], 1.0f };
        glUniform4fv(colorLoc, 1, (const GLfloat*)slimeSolidColor);

        // Prepara a matriz de transformação (posição e tamanho)
        glm_mat4_identity(model_matrix);
        glm_translate(model_matrix, (vec3){currentSlime->x, currentSlime->y, currentSlime->z});
        glm_scale(model_matrix, (vec3){currentSlime->raio * 2.0f, currentSlime->raio * 2.0f, 1.0f});
        
        // Envia a matriz para o shader e desenha o corpo
        glUniformMatrix4fv(modelLoc, 1, GL_FALSE, (const GLfloat*)model_matrix);
        glDrawElements(GL_TRIANGLES, 6, GL_UNSIGNED_INT, 0);
        
        // ===== PASSO 2: DESENHAR O ROSTO TEXTURIZADO POR CIMA =====
        glUniform1i(useTextureLoc, true); // Agora, usaremos uma textura
        glBindTexture(GL_TEXTURE_2D, slimeFaceTextureID);

        // A posição e o tamanho são os mesmos do corpo, então reutilizamos a `model_matrix`
        glUniformMatrix4fv(modelLoc, 1, GL_FALSE, (const GLfloat*)model_matrix);
        glDrawElements(GL_TRIANGLES, 6, GL_UNSIGNED_INT, 0);

        // ===== PASSO 3: DESENHAR A BARRA DE VIDA (como antes) =====
        glUniform1i(useTextureLoc, false); // Voltamos para cor sólida
        
        float bar_width = currentSlime->raio * 2.0f;
        float bar_height = 5.0f * g_ScaleFactor;
        float bar_y_offset = currentSlime->raio + (5.0f * g_ScaleFactor);

        // Barra de fundo (vermelha)
        glUniform4fv(colorLoc, 1, (const GLfloat*)health_bar_bg_color);
        mat4 model_health_bar_bg;
        glm_mat4_identity(model_health_bar_bg);
        glm_translate(model_health_bar_bg, (vec3){currentSlime->x, currentSlime->y + bar_y_offset, 0.0f});
        glm_scale(model_health_bar_bg, (vec3){bar_width, bar_height, 1.0f});
        glUniformMatrix4fv(modelLoc, 1, GL_FALSE, (const GLfloat*)model_health_bar_bg);
        glDrawElements(GL_TRIANGLES, 6, GL_UNSIGNED_INT, 0);
        
        // Barra de vida (verde)
        float health_percentage = currentSlime->health / currentSlime->maxHealth;
        if (health_percentage < 0) health_percentage = 0;
        float green_bar_width = bar_width * health_percentage;
        if (green_bar_width > 0) {
            glUniform4fv(colorLoc, 1, (const GLfloat*)health_bar_fg_color);
            mat4 model_health_bar_fg;
            glm_mat4_identity(model_health_bar_fg);
            float green_bar_center_x = currentSlime->x - (bar_width / 2.0f) + (green_bar_width / 2.0f);
            glm_translate(model_health_bar_fg, (vec3){green_bar_center_x, currentSlime->y + bar_y_offset, 0.0f});
            glm_scale(model_health_bar_fg, (vec3){green_bar_width, bar_height, 1.0f});
            glUniformMatrix4fv(modelLoc, 1, GL_FALSE, (const GLfloat*)model_health_bar_fg);
            glDrawElements(GL_TRIANGLES, 6, GL_UNSIGNED_INT, 0);
        }
    }
}

void updateSlimes(float deltaTime, Player* p) {
    // Itera pelo array de slimes ativos
    for (int i = 0; i < activeSlimesCount; ) {
        Slime* currentSlime = &slimes[i];
        bool slime_removed = false;

        // 1. Atualizar posição do slime
        float dirX = p->position[0] - currentSlime->x;
        float dirY = p->position[1] - currentSlime->y;
        float length = sqrtf(dirX * dirX + dirY * dirY);
        if (length > 0) {
            dirX /= length;
            dirY /= length;
        }
        currentSlime->x += dirX * SLIME_SPEED * deltaTime;
        currentSlime->y += dirY * SLIME_SPEED * deltaTime;

        // 2. Verificar se o slime deve ser removido (vida zerada OU colisão com jogador)
        
        // Condição de morte por falta de vida
        if (currentSlime->health <= 0) {
            currentSlime->ativo = false; // Marcar como inativo para a lógica abaixo
        }

        // Condição de morte por colisão com jogador
        float playerLeft   = p->position[0] - p->size[0] / 2.0f;
        float playerRight  = p->position[0] + p->size[0] / 2.0f;
        float playerBottom = p->position[1] - p->size[1] / 2.0f;
        float playerTop    = p->position[1] + p->size[1] / 2.0f;

        float slimeLeft   = currentSlime->x - currentSlime->raio;
        float slimeRight  = currentSlime->x + currentSlime->raio;
        float slimeBottom = currentSlime->y - currentSlime->raio;
        float slimeTop    = currentSlime->y + currentSlime->raio;

        if (playerLeft < slimeRight && playerRight > slimeLeft &&
            playerBottom < slimeTop && playerTop > slimeBottom)
        {
            p->health -= SLIME_COLLISION_DAMAGE;
            if (p->health <= 0) {
                p->health = 0;
                currentGameState = GAME_STATE_GAMEOVER;
            }
            currentSlime->ativo = false; // Marcar para remoção
        }
        
        // 3. Lógica de compactação: se o slime foi marcado como inativo, remova-o
        if (!currentSlime->ativo) {
              // ===== PONTO CHAVE: CONTAR A MORTE E VERIFICAR FASE =====
            g_KillsCount++; // Incrementa o contador de mortes
            printf("Kills: %d / Phase: %d\n", g_KillsCount, g_CurrentPhase); // Debug

            // Lógica de avanço de fase
            if (g_CurrentPhase == 1 && g_KillsCount >= KILLS_FOR_PHASE_2) {
                g_CurrentPhase = 2;
                g_KillsAtPhaseStart = KILLS_FOR_PHASE_2; // <-- ATUALIZA O PONTO DE PARTIDA
                printf("Avançou para a Fase 2!\n");
            } else if (g_CurrentPhase == 2 && g_KillsCount >= KILLS_FOR_PHASE_3) {
                g_CurrentPhase = 3;
                g_KillsAtPhaseStart = KILLS_FOR_PHASE_3; // <-- ATUALIZA O PONTO DE PARTIDA
                printf("Avançou para a Fase 3!\n");
            } else if (g_CurrentPhase == 3 && g_KillsCount >= KILLS_FOR_VICTORY) {
                printf("VITÓRIA!\n");
                currentGameState = GAME_STATE_VICTORY; 
            }
            // ==========================================================

            spawnParticleExplosion((vec2){currentSlime->x, currentSlime->y}, currentSlime->cor, 100);
            activeSlimesCount--;
            slimes[i] = slimes[activeSlimesCount];
            slime_removed = true; 
        }

        if (!slime_removed) {
            i++;
        }
    }
}


// Funcoes para INPUT, TEXTURAS E FUNCIONALIDADES DO JOGO
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
        if (glfwGetKey(window, GLFW_KEY_LEFT) == GLFW_PRESS) p->angle += rotation_change;
        if (glfwGetKey(window, GLFW_KEY_RIGHT) == GLFW_PRESS) p->angle -= rotation_change;

        // Disparo
        timeSinceLastShot += deltaTime;
        if (glfwGetKey(window, GLFW_KEY_SPACE) == GLFW_PRESS) {
            if (timeSinceLastShot >= FIRE_RATE) {
                fireProjectile(p);
                timeSinceLastShot = 0.0f;
            }
        }

        // ===== CORREÇÃO: LIMITAR O JOGADOR AOS LIMITES DA TELA ATUAL =====
        float half_width = p->size[0] / 2.0f;
        float half_height = p->size[1] / 2.0f;
        if (p->position[0] - half_width < 0.0f) p->position[0] = half_width;
        if (p->position[0] + half_width > g_CurrentScreenWidth) p->position[0] = g_CurrentScreenWidth - half_width;
        if (p->position[1] - half_height < 0.0f) p->position[1] = half_height;
        if (p->position[1] + half_height > g_CurrentScreenHeight) p->position[1] = g_CurrentScreenHeight - half_height;
    }
    else if (currentGameState == GAME_STATE_GAMEOVER) {
        if (glfwGetKey(window, GLFW_KEY_R) == GLFW_PRESS) {
            resetGame();
        }

    // Verifica se a tecla 'M' foi pressionada para voltar ao menu
        if (glfwGetKey(window, GLFW_KEY_M) == GLFW_PRESS) {
            currentGameState = GAME_STATE_MENU;
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

unsigned int loadTexture(const char *path) {
    unsigned int textureID;
    glGenTextures(1, &textureID);

    int width, height, nrComponents;
   
    stbi_set_flip_vertically_on_load(true); 
   
    unsigned char *data = stbi_load(path, &width, &height, &nrComponents, 0);
   
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
    printf("Iniciando/Reiniciando jogo com dificuldade: %d\n", selectedDifficulty);

    // ===== RESETAR VARIÁVEIS DE FASE =====
    g_CurrentPhase = 1;
    g_KillsCount = 0;
    g_KillsAtPhaseStart = 0;
    g_ShowingVictoryScreen = false;
    g_VictoryScreenTimer = 10.0f;
    // ===================================

    // Aplicar configurações de dificuldade
    if (selectedDifficulty == DIFFICULTY_EASY) {
        SLIME_MAX_HEALTH = 80.0f;
        SLIME_SPEED = 80.0f;
        spawnInterval = 4.0f;
    } else if (selectedDifficulty == DIFFICULTY_NORMAL) {
        SLIME_MAX_HEALTH = 100.0f;
        SLIME_SPEED = 100.0f;
        spawnInterval = 3.0f;
    } else if (selectedDifficulty == DIFFICULTY_HARD) {
        SLIME_MAX_HEALTH = 150.0f;
        SLIME_SPEED = 130.0f;
        spawnInterval = 2.0f;
    }

   // Resetar jogador
    player.position[0] = g_CurrentScreenWidth / 2.0f;
    player.position[1] = g_CurrentScreenHeight / 2.0f;
    player.angle = 0.0f;
    player.health = PLAYER_MAX_HEALTH;
    player.maxHealth = PLAYER_MAX_HEALTH;
    
    // ===== CORREÇÃO: APLICAR FATOR DE ESCALA AO TAMANHO DO JOGADOR =====
    player.size[0] = 50.0f * g_ScaleFactor;
    player.size[1] = 50.0f * g_ScaleFactor;

    initializeSlimes();
    initializeProjectiles();
    initializeParticles();
    timeSinceLastSpawn = 0.0f;
    timeSinceLastShot = 0.0f;
    currentGameState = GAME_STATE_PLAYING;
}


// Funcoes para PROJETEIS
void initializeProjectiles() {
    for (int i = 0; i < MAX_PROJECTILES; ++i) {
        projectiles[i].active = false;
    }
    // activeProjectilesCount = 0; // Se for usar um contador de ativos específico para projéteis
}

void drawProjectiles(unsigned int currentShaderProgram, unsigned int currentVAO, mat4 model_matrix_ref_placeholder) {
    mat4 model_proj_m;
    unsigned int modelLoc = glGetUniformLocation(currentShaderProgram, "model");
    unsigned int colorLoc = glGetUniformLocation(currentShaderProgram, "objectColor_Solid");

    // ===== CORREÇÃO: MUDAR DE VEC3 PARA VEC4 =====
    // Adicionar 1.0f para o canal Alpha (opacidade total)
    vec4 projectile_color = {1.0f, 1.0f, 0.0f, 1.0f};
    glUniform4fv(colorLoc, 1, (const GLfloat*)projectile_color); // Usar glUniform4fv

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
    int projectileIndex = -1;
    for (int i = 0; i < MAX_PROJECTILES; ++i) {
        if (!projectiles[i].active) {
            projectileIndex = i;
            break;
        }
    }

    if (projectileIndex == -1) { return; }

    Projectile* proj = &projectiles[projectileIndex];

    proj->active = true;
    proj->speed = PROJECTILE_SPEED;
    // ===== CORREÇÃO: APLICAR FATOR DE ESCALA AO TAMANHO DO PROJÉTIL =====
    proj->size = PROJECTILE_SIZE * g_ScaleFactor;
    proj->angle = p->angle;
    proj->lifetime = PROJECTILE_LIFETIME;

    // A lógica de offset da arma já usa player.size, que agora é escalado,
    // então o cálculo do ponto de disparo já está correto.
    vec2 start_pos;
    glm_vec2_copy(p->position, start_pos);
    float dirX = cosf(p->angle);
    float dirY = sinf(p->angle);
    float weapon_actual_width = p->size[0] * 0.6f;
    float weapon_offset_from_player_center = p->size[0] * 0.4f;
    float gun_tip_offset = weapon_offset_from_player_center + (weapon_actual_width / 2.0f) + (proj->size / 2.0f);
    start_pos[0] += dirX * gun_tip_offset;
    start_pos[1] += dirY * gun_tip_offset;
    glm_vec2_copy(start_pos, proj->position);
    
    proj->velocity[0] = dirX * proj->speed;
    proj->velocity[1] = dirY * proj->speed;
}

void updateProjectiles(float deltaTime) {
    for (int i = 0; i < MAX_PROJECTILES; ++i) {
        if (projectiles[i].active) {
            Projectile* proj = &projectiles[i];

            proj->position[0] += proj->velocity[0] * deltaTime;
            proj->position[1] += proj->velocity[1] * deltaTime;

            proj->lifetime -= deltaTime;
            if (proj->lifetime <= 0.0f) {
                proj->active = false;
                continue;
            }

            // ===== CORREÇÃO: VERIFICAR LIMITES DA TELA ATUAL =====
            if (proj->position[0] < 0 || proj->position[0] > g_CurrentScreenWidth ||
                proj->position[1] < 0 || proj->position[1] > g_CurrentScreenHeight) {
                proj->active = false;
                continue;
            }

            // Colisão Projétil-Slime (sem alterações aqui)
            for (int j = 0; j < MAX_SLIMES; ++j) { // Itera sobre todo o array
                if (slimes[j].ativo) {
                    Slime* currentSlime = &slimes[j];

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
                        
                        proj->active = false;
                        currentSlime->health -= PROJECTILE_DAMAGE;

                        break;
                    }
                }
            }
        }
    }
}


// Funcoes para MOUSE E INTERAÇÕES DO MENU
void mouse_pos_callback(GLFWwindow* window, double xpos, double ypos) {
    mouseX = xpos;
    // mouseY = SCR_HEIGHT - ypos; // Inverte Y
    mouseY = g_CurrentScreenHeight - ypos; // NOVA LINHA (inverte Y usando altura atual)
}

void mouse_button_callback(GLFWwindow* window, int button, int action, int mods) {
    if (currentGameState != GAME_STATE_MENU) return;

    if (button == GLFW_MOUSE_BUTTON_LEFT && action == GLFW_PRESS) {
        if (startButton.is_hovered) {
            printf("Botao Iniciar clicado!\n");
            resetGame();
        }
        
        // Vamos usar um único botão para ciclar a dificuldade
        if (difficultyButton_Normal.is_hovered) { // Checa o hover em qualquer um dos botões de dificuldade
            selectedDifficulty = (selectedDifficulty + 1) % 3; // Cicla EASY -> NORMAL -> HARD -> EASY
            printf("Dificuldade mudada para: %d\n", selectedDifficulty);
        }
    }
}

void checkMenuButtonHovers() {
    // Botão Iniciar
    float start_left = startButton.x - startButton.width / 2.0f;
    float start_right = startButton.x + startButton.width / 2.0f;
    float start_bottom = startButton.y - startButton.height / 2.0f;
    float start_top = startButton.y + startButton.height / 2.0f;
    startButton.is_hovered = (mouseX > start_left && mouseX < start_right && mouseY > start_bottom && mouseY < start_top);

    // Botão Dificuldade (a área clicável é a mesma para todos os 3 estados)
    float diff_left = difficultyButton_Normal.x - difficultyButton_Normal.width / 2.0f;
    float diff_right = difficultyButton_Normal.x + difficultyButton_Normal.width / 2.0f;
    float diff_bottom = difficultyButton_Normal.y - difficultyButton_Normal.height / 2.0f;
    float diff_top = difficultyButton_Normal.y + difficultyButton_Normal.height / 2.0f;
    
    bool is_hovering_difficulty = (mouseX > diff_left && mouseX < diff_right && mouseY > diff_bottom && mouseY < diff_top);
    difficultyButton_Easy.is_hovered = is_hovering_difficulty;
    difficultyButton_Normal.is_hovered = is_hovering_difficulty;
    difficultyButton_Hard.is_hovered = is_hovering_difficulty;
}


// Funcoes para PARTICULAS
void initializeParticles() {
    for (int i = 0; i < MAX_PARTICLES; ++i) {
        particles[i].active = false;
    }
}

unsigned int firstInactiveParticle() {
    for (int i = 0; i < MAX_PARTICLES; ++i) {
        if (!particles[i].active) {
            return i;
        }
    }
    // Se não encontrar, reutiliza a primeira (fallback)
    return 0;
}

void spawnParticleExplosion(vec2 position, vec3 baseColor, int count) {
    for (int i = 0; i < count; i++) {
        unsigned int p_idx = firstInactiveParticle();
        Particle* p = &particles[p_idx];

        p->active = true;
        p->life = PARTICLE_LIFETIME;
        glm_vec2_copy(position, p->position);

        // Cor inicial é a do slime, com opacidade total
        p->color[0] = baseColor[0];
        p->color[1] = baseColor[1];
        p->color[2] = baseColor[2];
        p->color[3] = 1.0f; // Alpha

        // Velocidade aleatória em todas as direções
        float angle = ((float)rand() / (float)RAND_MAX) * 2.0f * 3.14159f;
        float speed = PARTICLE_SPEED * (0.5f + ((float)rand() / (float)RAND_MAX) * 0.5f);
        p->velocity[0] = cosf(angle) * speed * g_ScaleFactor;
        p->velocity[1] = sinf(angle) * speed * g_ScaleFactor;
    }
}

void updateParticles(float deltaTime) {
    for (int i = 0; i < MAX_PARTICLES; ++i) {
        if (particles[i].active) {
            Particle* p = &particles[i];
            
            p->life -= deltaTime;
            if (p->life <= 0.0f) {
                p->active = false;
                continue;
            }

            p->position[0] += p->velocity[0] * deltaTime;
            p->position[1] += p->velocity[1] * deltaTime;
            
            // A transparência (alpha) diminui conforme a vida acaba
            p->color[3] = p->life / PARTICLE_LIFETIME;
        }
    }
}

void drawParticles(unsigned int shaderProgram, unsigned int vao) {
    glUniform1i(glGetUniformLocation(shaderProgram, "useTexture"), false);
    unsigned int modelLoc = glGetUniformLocation(shaderProgram, "model");
    unsigned int colorLoc = glGetUniformLocation(shaderProgram, "objectColor_Solid");

    glBindVertexArray(vao);
    for (int i = 0; i < MAX_PARTICLES; ++i) {
        if (particles[i].active) {
            Particle* p = &particles[i];
            
            // Envia a cor com transparência para o shader
            glUniform4fv(colorLoc, 1, p->color);
            
            mat4 model_particle;
            glm_mat4_identity(model_particle);
            glm_translate(model_particle, (vec3){p->position[0], p->position[1], 0.0f});
            
            // Partículas são pequenos quadrados
            float particleSize = 3.0f * g_ScaleFactor;
            glm_scale(model_particle, (vec3){particleSize, particleSize, 1.0f});
            
            glUniformMatrix4fv(modelLoc, 1, GL_FALSE, (const GLfloat*)model_particle);
            glDrawElements(GL_TRIANGLES, 6, GL_UNSIGNED_INT, 0);
        }
    }
}

