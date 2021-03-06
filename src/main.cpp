#include "imgui.h"
#include "imgui_impl_glfw.h"
#include "imgui_impl_opengl3.h"

#include <glad/glad.h>
#include <GLFW/glfw3.h>
#include <stb_image.h>

#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/type_ptr.hpp>

#include <learnopengl/filesystem.h>
#include <learnopengl/shader.h>
#include <learnopengl/camera.h>
#include <learnopengl/model.h>

#include <iostream>

void framebuffer_size_callback(GLFWwindow *window, int width, int height);

void mouse_callback(GLFWwindow *window, double xpos, double ypos);

void scroll_callback(GLFWwindow *window, double xoffset, double yoffset);

void processInput(GLFWwindow *window);

void key_callback(GLFWwindow *window, int key, int scancode, int action, int mods);

unsigned int loadTexture(const char *path, bool gammaCorrection);

void renderQuad();

void renderCube();

unsigned int loadCubemap(vector<std::string> faces);

// settings
const unsigned int SCR_WIDTH = 1600;
const unsigned int SCR_HEIGHT = 1200;
bool hdr = true;
bool hdrKeyPressed = false;
float exposure = 1.5f;

// camera

double lastX = SCR_WIDTH / 2.0f;
double lastY = SCR_HEIGHT / 2.0f;
bool firstMouse = true;

// timing
float deltaTime = 0.0f;
float lastFrame = 0.0f;

struct PointLight {
    glm::vec3 position;
    glm::vec3 ambient;
    glm::vec3 diffuse;
    glm::vec3 specular;

    float constant;
    float linear;
    float quadratic;
};

struct DirLight {
    glm::vec3 direction;

    glm::vec3 ambient;
    glm::vec3 diffuse;
    glm::vec3 specular;
};

struct SpotLight {
    glm::vec3 position;
    glm::vec3 direction;
    float cutOff;
    float outerCutOff;

    float constant;
    float linear;
    float quadratic;

    glm::vec3 ambient;
    glm::vec3 diffuse;
    glm::vec3 specular;
};

struct ProgramState {
    bool ImGuiEnabled = false;
    Camera camera;
    bool CameraMouseMovementUpdateEnabled = true;
    bool PointLightEnabled = false;
    bool SpotLightEnabled = true;

    PointLight pointLight;
    DirLight dirLight;
    SpotLight spotLight;

    ProgramState()
            : camera(glm::vec3(0.0f, 0.0f, 3.0f)) {}

    void SaveToFile(std::string filename);

    void LoadFromFile(std::string filename);
};

void ProgramState::SaveToFile(std::string filename) {
    std::ofstream out(filename);
    out << ImGuiEnabled << '\n'
        << camera.Position.x << '\n'
        << camera.Position.y << '\n'
        << camera.Position.z << '\n'
        << camera.Front.x << '\n'
        << camera.Front.y << '\n'
        << camera.Front.z << '\n';
}

void ProgramState::LoadFromFile(std::string filename) {
    std::ifstream in(filename);
    if (in) {
        in >> ImGuiEnabled
           >> camera.Position.x
           >> camera.Position.y
           >> camera.Position.z
           >> camera.Front.x
           >> camera.Front.y
           >> camera.Front.z;
    }
}

ProgramState *programState;

void DrawImGui(ProgramState *programState);

int main() {
    // glfw: initialize and configure
    // ------------------------------
    glfwInit();
    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 3);
    glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);

#ifdef __APPLE__
    glfwWindowHint(GLFW_OPENGL_FORWARD_COMPAT, GL_TRUE);
#endif

    // glfw window creation
    // --------------------
    GLFWwindow *window = glfwCreateWindow(SCR_WIDTH, SCR_HEIGHT, "LearnOpenGL", NULL, NULL);
    if (window == NULL) {
        std::cout << "Failed to create GLFW window" << std::endl;
        glfwTerminate();
        return -1;
    }
    glfwMakeContextCurrent(window);
    glfwSetFramebufferSizeCallback(window, framebuffer_size_callback);
    glfwSetCursorPosCallback(window, mouse_callback);
    glfwSetScrollCallback(window, scroll_callback);
    glfwSetKeyCallback(window, key_callback);
    // tell GLFW to capture our mouse
    glfwSetInputMode(window, GLFW_CURSOR, GLFW_CURSOR_NORMAL); //ne radi sa GLFW_CURSOR_DISABLED
    // glad: load all OpenGL function pointers
    // ---------------------------------------
    if (!gladLoadGLLoader((GLADloadproc) glfwGetProcAddress)) {
        std::cout << "Failed to initialize GLAD" << std::endl;
        return -1;
    }

    //DO NOT tell stb_image.h to flip loaded texture's on the y-axis (before loading model).
    stbi_set_flip_vertically_on_load(false); //work for all model maps and cubemap

    programState = new ProgramState;
    programState->LoadFromFile("resources/program_state.txt");
    if (programState->ImGuiEnabled) {
        glfwSetInputMode(window, GLFW_CURSOR, GLFW_CURSOR_NORMAL);
    }
    // Init Imgui
    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    ImGuiIO &io = ImGui::GetIO();
    (void) io;

    ImGui_ImplGlfw_InitForOpenGL(window, true);
    ImGui_ImplOpenGL3_Init("#version 330 core");

    // configure global opengl state
    // -----------------------------
    glEnable(GL_DEPTH_TEST);
    glEnable(GL_CULL_FACE);
    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
    //glFrontFace(GL_CW);

    // build and compile shaders
    // -------------------------
    Shader roomShader("resources/shaders/room.vs", "resources/shaders/room.fs");
    Shader cubemapShader("resources/shaders/cubemap.vs", "resources/shaders/cubemap.fs");
    Shader hdrShader("resources/shaders/hdr.vs", "resources/shaders/hdr.fs");

    // configure floating point framebuffer
    // ------------------------------------
    unsigned int hdrFBO;
    glGenFramebuffers(1, &hdrFBO);
    // create floating point color buffer
    unsigned int colorBuffer;
    glGenTextures(1, &colorBuffer);
    glBindTexture(GL_TEXTURE_2D, colorBuffer);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA16F, SCR_WIDTH, SCR_HEIGHT, 0, GL_RGBA, GL_FLOAT, NULL);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    // create depth buffer (renderbuffer)
    unsigned int rboDepth;
    glGenRenderbuffers(1, &rboDepth);
    glBindRenderbuffer(GL_RENDERBUFFER, rboDepth);
    glRenderbufferStorage(GL_RENDERBUFFER, GL_DEPTH_COMPONENT, SCR_WIDTH, SCR_HEIGHT);
    // attach buffers
    glBindFramebuffer(GL_FRAMEBUFFER, hdrFBO);
    glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, colorBuffer, 0);
    glFramebufferRenderbuffer(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, GL_RENDERBUFFER, rboDepth);
    if (glCheckFramebufferStatus(GL_FRAMEBUFFER) != GL_FRAMEBUFFER_COMPLETE)
        std::cout << "Framebuffer not complete!" << std::endl;
    glBindFramebuffer(GL_FRAMEBUFFER, 0);


    float cubeVertices[] = { // from learn opengl
            //positions          // normals           // texture coords
            -0.5f, -0.5f, -0.5f,  0.0f,  0.0f, -1.0f,  0.0f,  0.0f,
            0.5f,  0.5f, -0.5f,  0.0f,  0.0f, -1.0f,  1.0f,  1.0f,
            0.5f, -0.5f, -0.5f,  0.0f,  0.0f, -1.0f,  1.0f,  0.0f,
            -0.5f,  0.5f, -0.5f,  0.0f,  0.0f, -1.0f,  0.0f,  1.0f,
            0.5f,  0.5f, -0.5f,  0.0f,  0.0f, -1.0f,  1.0f,  1.0f,
            -0.5f, -0.5f, -0.5f,  0.0f,  0.0f, -1.0f,  0.0f,  0.0f,

            -0.5f, -0.5f,  0.5f,  0.0f,  0.0f,  1.0f,  0.0f,  0.0f,
            0.5f, -0.5f,  0.5f,  0.0f,  0.0f,  1.0f,  1.0f,  0.0f,
            0.5f,  0.5f,  0.5f,  0.0f,  0.0f,  1.0f,  1.0f,  1.0f,
            -0.5f, -0.5f,  0.5f,  0.0f,  0.0f,  1.0f,  0.0f,  0.0f,
            0.5f,  0.5f,  0.5f,  0.0f,  0.0f,  1.0f,  1.0f,  1.0f,
            -0.5f,  0.5f,  0.5f,  0.0f,  0.0f,  1.0f,  0.0f,  1.0f,

            -0.5f,  0.5f,  0.5f, -1.0f,  0.0f,  0.0f,  1.0f,  0.0f,
            -0.5f,  0.5f, -0.5f, -1.0f,  0.0f,  0.0f,  1.0f,  1.0f,
            -0.5f, -0.5f, -0.5f, -1.0f,  0.0f,  0.0f,  0.0f,  1.0f,
            -0.5f,  0.5f,  0.5f, -1.0f,  0.0f,  0.0f,  1.0f,  0.0f,
            -0.5f, -0.5f, -0.5f, -1.0f,  0.0f,  0.0f,  0.0f,  1.0f,
            -0.5f, -0.5f,  0.5f, -1.0f,  0.0f,  0.0f,  0.0f,  0.0f,

            0.5f,  0.5f,  0.5f,  1.0f,  0.0f,  0.0f,  1.0f,  0.0f,
            0.5f, -0.5f, -0.5f,  1.0f,  0.0f,  0.0f,  0.0f,  1.0f,
            0.5f,  0.5f, -0.5f,  1.0f,  0.0f,  0.0f,  1.0f,  1.0f,
            0.5f, -0.5f,  0.5f,  1.0f,  0.0f,  0.0f,  0.0f,  0.0f,
            0.5f, -0.5f, -0.5f,  1.0f,  0.0f,  0.0f,  0.0f,  1.0f,
            0.5f,  0.5f,  0.5f,  1.0f,  0.0f,  0.0f,  1.0f,  0.0f,

            -0.5f, -0.5f, -0.5f,  0.0f, -1.0f,  0.0f,  0.0f,  1.0f,
            0.5f, -0.5f, -0.5f,  0.0f, -1.0f,  0.0f,  1.0f,  1.0f,
            0.5f, -0.5f,  0.5f,  0.0f, -1.0f,  0.0f,  1.0f,  0.0f,
            -0.5f, -0.5f, -0.5f,  0.0f, -1.0f,  0.0f,  0.0f,  1.0f,
            0.5f, -0.5f,  0.5f,  0.0f, -1.0f,  0.0f,  1.0f,  0.0f,
            -0.5f, -0.5f,  0.5f,  0.0f, -1.0f,  0.0f,  0.0f,  0.0f,

            -0.5f,  0.5f, -0.5f,  0.0f,  1.0f,  0.0f,  0.0f,  1.0f,
            0.5f,  0.5f,  0.5f,  0.0f,  1.0f,  0.0f,  1.0f,  0.0f,
            0.5f,  0.5f, -0.5f,  0.0f,  1.0f,  0.0f,  1.0f,  1.0f,
            -0.5f,  0.5f,  0.5f,  0.0f,  1.0f,  0.0f,  0.0f,  0.0f,
            0.5f,  0.5f,  0.5f,  0.0f,  1.0f,  0.0f,  1.0f,  0.0f,
            -0.5f,  0.5f, -0.5f,  0.0f,  1.0f,  0.0f,  0.0f,  1.0f
    };

    float floorVertices[] = { // cube from learn opengl, normals inverted, texture coords adjusted
            -0.5f, -0.5f, -0.5f,  0.0f, 1.0f,  0.0f,  0.0f,  6.0f,
            0.5f, -0.5f,  0.5f,  0.0f, 1.0f,  0.0f,  6.0f,  0.0f,
            0.5f, -0.5f, -0.5f,  0.0f, 1.0f,  0.0f,  6.0f,  6.0f,
            -0.5f, -0.5f,  0.5f,  0.0f, 1.0f,  0.0f,  0.0f,  0.0f,
            0.5f, -0.5f,  0.5f,  0.0f, 1.0f,  0.0f,  6.0f,  0.0f,
            -0.5f, -0.5f, -0.5f,  0.0f, 1.0f,  0.0f,  0.0f,  6.0f
    };

    float wallsVertices[] = { // cube from learn opengl, normals inverted, texture coords adjusted
            -0.5f, -0.5f, -0.5f,  0.0f,  0.0f, 1.0f,  0.0f,  0.0f,
            0.5f, -0.5f, -0.5f,  0.0f,  0.0f, 1.0f,  1.0f,  0.0f,
            0.5f,  0.5f, -0.5f,  0.0f,  0.0f, 1.0f,  1.0f,  1.0f,
            0.5f,  0.5f, -0.5f,  0.0f,  0.0f, 1.0f,  1.0f,  1.0f,
            -0.5f,  0.5f, -0.5f,  0.0f,  0.0f, 1.0f,  0.0f,  1.0f,
            -0.5f, -0.5f, -0.5f,  0.0f,  0.0f, 1.0f,  0.0f,  0.0f,

            -0.5f, -0.5f,  0.5f,  0.0f,  0.0f,  -1.0f,  0.0f,  0.0f,
            0.5f,  0.5f,  0.5f,  0.0f,  0.0f,  -1.0f,  1.0f,  1.0f,
            0.5f, -0.5f,  0.5f,  0.0f,  0.0f,  -1.0f,  1.0f,  0.0f,
            -0.5f,  0.5f,  0.5f,  0.0f,  0.0f,  -1.0f,  0.0f,  1.0f,
            0.5f,  0.5f,  0.5f,  0.0f,  0.0f,  -1.0f,  1.0f,  1.0f,
            -0.5f, -0.5f,  0.5f,  0.0f,  0.0f,  -1.0f,  0.0f,  0.0f,

            -0.5f,  0.5f,  0.5f, 1.0f,  0.0f,  0.0f,  1.0f,  0.0f,
            -0.5f, -0.5f, -0.5f, 1.0f,  0.0f,  0.0f,  0.0f,  1.0f,
            -0.5f,  0.5f, -0.5f, 1.0f,  0.0f,  0.0f,  1.0f,  1.0f,
            -0.5f, -0.5f,  0.5f, 1.0f,  0.0f,  0.0f,  0.0f,  0.0f,
            -0.5f, -0.5f, -0.5f, 1.0f,  0.0f,  0.0f,  0.0f,  1.0f,
            -0.5f,  0.5f,  0.5f, 1.0f,  0.0f,  0.0f,  1.0f,  0.0f,

            0.5f,  0.5f,  0.5f,  -1.0f,  0.0f,  0.0f,  1.0f,  0.0f,
            0.5f,  0.5f, -0.5f,  -1.0f,  0.0f,  0.0f,  1.0f,  1.0f,
            0.5f, -0.5f, -0.5f,  -1.0f,  0.0f,  0.0f,  0.0f,  1.0f,
            0.5f, -0.5f, -0.5f,  -1.0f,  0.0f,  0.0f,  0.0f,  1.0f,
            0.5f, -0.5f,  0.5f,  -1.0f,  0.0f,  0.0f,  0.0f,  0.0f,
            0.5f,  0.5f,  0.5f,  -1.0f,  0.0f,  0.0f,  1.0f,  0.0f
    };

    glm::vec3 tableLegsPosition[] = {
            glm::vec3(3.0f, 1.0f, -2.0f),
            glm::vec3(3.0f, 1.0f, 5.0f),
            glm::vec3(-6.0f, 1.0f, 5.0f),
            glm::vec3(-6.0f, 1.0f, -2.0f)
    };

    float glassVertices[] = { // cube from learn opengl, normals inverted
            -0.5f, -0.5f, -0.5f,  0.0f, 1.0f,  0.0f,  0.0f,  1.0f,
            0.5f, -0.5f,  0.5f,  0.0f, 1.0f,  0.0f,  1.0f,  0.0f,
            0.5f, -0.5f, -0.5f,  0.0f, 1.0f,  0.0f,  1.0f,  1.0f,
            -0.5f, -0.5f,  0.5f,  0.0f, 1.0f,  0.0f,  0.0f,  0.0f,
            0.5f, -0.5f,  0.5f,  0.0f, 1.0f,  0.0f,  1.0f,  0.0f,
            -0.5f, -0.5f, -0.5f,  0.0f, 1.0f,  0.0f,  0.0f,  1.0f
    };

    float cubemapVertices[] = {
            // positions
            -1.0f,  1.0f, -1.0f,
            -1.0f, -1.0f, -1.0f,
            1.0f, -1.0f, -1.0f,
            1.0f, -1.0f, -1.0f,
            1.0f,  1.0f, -1.0f,
            -1.0f,  1.0f, -1.0f,

            -1.0f, -1.0f,  1.0f,
            -1.0f, -1.0f, -1.0f,
            -1.0f,  1.0f, -1.0f,
            -1.0f,  1.0f, -1.0f,
            -1.0f,  1.0f,  1.0f,
            -1.0f, -1.0f,  1.0f,

            1.0f, -1.0f, -1.0f,
            1.0f, -1.0f,  1.0f,
            1.0f,  1.0f,  1.0f,
            1.0f,  1.0f,  1.0f,
            1.0f,  1.0f, -1.0f,
            1.0f, -1.0f, -1.0f,

            -1.0f, -1.0f,  1.0f,
            -1.0f,  1.0f,  1.0f,
            1.0f,  1.0f,  1.0f,
            1.0f,  1.0f,  1.0f,
            1.0f, -1.0f,  1.0f,
            -1.0f, -1.0f,  1.0f,

            -1.0f,  1.0f, -1.0f,
            1.0f,  1.0f, -1.0f,
            1.0f,  1.0f,  1.0f,
            1.0f,  1.0f,  1.0f,
            -1.0f,  1.0f,  1.0f,
            -1.0f,  1.0f, -1.0f,

            -1.0f, -1.0f, -1.0f,
            -1.0f, -1.0f,  1.0f,
            1.0f, -1.0f, -1.0f,
            1.0f, -1.0f, -1.0f,
            -1.0f, -1.0f,  1.0f,
            1.0f, -1.0f,  1.0f
    };

    //cubemap

    unsigned int cubemapVAO, cubemapVBO;
    glGenVertexArrays(1, &cubemapVAO);
    glGenBuffers(1, &cubemapVBO);

    glBindVertexArray(cubemapVAO);
    glBindBuffer(GL_ARRAY_BUFFER, cubemapVBO);

    glBufferData(GL_ARRAY_BUFFER, sizeof(cubemapVertices), &cubemapVertices, GL_STATIC_DRAW);

    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 3 * sizeof(float), (void*)0);
    glEnableVertexAttribArray(0);

    vector<std::string> faces
            {
                    FileSystem::getPath("resources/textures/brudslojan/posx.jpg"),
                    FileSystem::getPath("resources/textures/brudslojan/negx.jpg"),
                    FileSystem::getPath("resources/textures/brudslojan/posy.jpg"),
                    FileSystem::getPath("resources/textures/brudslojan/negy.jpg"),
                    FileSystem::getPath("resources/textures/brudslojan/posz.jpg"),
                    FileSystem::getPath("resources/textures/brudslojan/negz.jpg")
            };
    unsigned int cubemapTexture = loadCubemap(faces);

    //floor
    unsigned int floorVBO, floorVAO;
    glGenVertexArrays(1, &floorVAO);
    glGenBuffers(1, &floorVBO);

    glBindVertexArray(floorVAO);
    glBindBuffer(GL_ARRAY_BUFFER, floorVBO);

    glBufferData(GL_ARRAY_BUFFER, sizeof(floorVertices), floorVertices, GL_STATIC_DRAW);

    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 8 * sizeof(float), (void*)0);
    glEnableVertexAttribArray(0);
    glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, 8 * sizeof(float), (void*)(3 * sizeof(float)));
    glEnableVertexAttribArray(1);
    glVertexAttribPointer(2, 2, GL_FLOAT, GL_FALSE, 8 * sizeof(float), (void*)(6 * sizeof(float)));
    glEnableVertexAttribArray(2);

    unsigned int floorDiffuseMap = loadTexture(FileSystem::getPath("resources/textures/wood_floor_diffuse.jpg").c_str(), true);
    unsigned int floorSpecularMap = loadTexture(FileSystem::getPath("resources/textures/wood_floor_specular.jpg").c_str(), true);

    //walls
    unsigned int wallsVBO, wallsVAO;
    glGenVertexArrays(1, &wallsVAO);
    glGenBuffers(1, &wallsVBO);

    glBindVertexArray(wallsVAO);
    glBindBuffer(GL_ARRAY_BUFFER, wallsVBO);

    glBufferData(GL_ARRAY_BUFFER, sizeof(wallsVertices), wallsVertices, GL_STATIC_DRAW);

    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 8 * sizeof(float), (void*)0);
    glEnableVertexAttribArray(0);
    glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, 8 * sizeof(float), (void*)(3 * sizeof(float)));
    glEnableVertexAttribArray(1);
    glVertexAttribPointer(2, 2, GL_FLOAT, GL_FALSE, 8 * sizeof(float), (void*)(6 * sizeof(float)));
    glEnableVertexAttribArray(2);

    unsigned int wallsDiffuseMap = loadTexture(FileSystem::getPath("resources/textures/wall_diffuse.jpg").c_str(), true);
    unsigned int wallsSpecularMap = loadTexture(FileSystem::getPath("resources/textures/wall_specular.jpg").c_str(), true);

    //table legs

    unsigned int cubeVBO, cubeVAO;
    glGenVertexArrays(1, &cubeVAO);
    glGenBuffers(1, &cubeVBO);

    glBindVertexArray(cubeVAO);
    glBindBuffer(GL_ARRAY_BUFFER, cubeVBO);

    glBufferData(GL_ARRAY_BUFFER, sizeof(cubeVertices), cubeVertices, GL_STATIC_DRAW);

    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 8 * sizeof(float), (void*)0);
    glEnableVertexAttribArray(0);
    glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, 8 * sizeof(float), (void*)(3 * sizeof(float)));
    glEnableVertexAttribArray(1);
    glVertexAttribPointer(2, 2, GL_FLOAT, GL_FALSE, 8 * sizeof(float), (void*)(6 * sizeof(float)));
    glEnableVertexAttribArray(2);

    unsigned int woodDiffuseMap = loadTexture(FileSystem::getPath("resources/textures/wood_diffuse2.jpg").c_str(), true);
    unsigned int woodSpecularMap = loadTexture(FileSystem::getPath("resources/textures/wood_specular2.jpg").c_str(), true);

    //table glass

    unsigned int glassVBO, glassVAO;
    glGenVertexArrays(1, &glassVAO);
    glGenBuffers(1, &glassVBO);

    glBindVertexArray(glassVAO);
    glBindBuffer(GL_ARRAY_BUFFER, glassVBO);

    glBufferData(GL_ARRAY_BUFFER, sizeof(glassVertices), glassVertices, GL_STATIC_DRAW);

    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 8 * sizeof(float), (void*)0);
    glEnableVertexAttribArray(0);
    glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, 8 * sizeof(float), (void*)(3 * sizeof(float)));
    glEnableVertexAttribArray(1);
    glVertexAttribPointer(2, 2, GL_FLOAT, GL_FALSE, 8 * sizeof(float), (void*)(6 * sizeof(float)));
    glEnableVertexAttribArray(2);

    unsigned int glassDiffuseMap = loadTexture(FileSystem::getPath("resources/textures/glass.png").c_str(), true);
    unsigned int glassSpecularMap = loadTexture(FileSystem::getPath("resources/textures/glass_specular.jpg").c_str(), true);


    // load models
    // -----------


    Model catModel("resources/objects/cat/12221_Cat_v1_l3.obj");
    catModel.SetShaderTextureNamePrefix("material.");

    Model lampModel("resources/objects/lamp/light.obj");
    lampModel.SetShaderTextureNamePrefix("material.");

    unsigned int lampTexture = loadTexture(FileSystem::getPath("resources/objects/lamp/lamp.jpg").c_str(), true);

    Model tvModel("resources/objects/Samsung_Smart_TV_55_Zoll/Samsung Smart TV 55 Zoll.obj");
    tvModel.SetShaderTextureNamePrefix("material.");

    Model sofaModel("resources/objects/sofa/3LU_KOLTUK.obj");
    sofaModel.SetShaderTextureNamePrefix("material.");

    unsigned int sofaDiffuse = loadTexture(FileSystem::getPath("resources/objects/sofa/sofa_diffuse.jpg").c_str(), true);
    unsigned int sofaSpecular = loadTexture(FileSystem::getPath("resources/objects/sofa/sofa_specular.jpg").c_str(), true);

    PointLight& pointLight = programState->pointLight;
    pointLight.position = glm::vec3(0.0f, 8.0f, 0.0f);
    pointLight.ambient = glm::vec3(0.9f, 0.9f, 0.9f);
    pointLight.diffuse = glm::vec3(5.6f, 5.6f, 5.6f);
    pointLight.specular = glm::vec3(4.0f, 4.0f, 4.0f);
    pointLight.constant = 1.0f;
    pointLight.linear = 0.09f;
    pointLight.quadratic = 0.032f;

    DirLight& dirLight = programState->dirLight;
    dirLight.direction = glm::vec3(0.0f, -1.0f, 0.0f);
    dirLight.ambient = glm::vec3(0.01f, 0.01f, 0.01f);
    dirLight.diffuse = glm::vec3(0.2f, 0.2f, 0.2f);
    dirLight.specular = glm::vec3(0.2f, 0.2f, 0.2f);

    SpotLight& spotLight = programState->spotLight;
    spotLight.direction = glm::vec3(0.0f, -1.0f, 0.0f);
    spotLight.position = glm::vec3(0.0f, 8.0f, 0.0f);
    spotLight.ambient = glm::vec3(0.1f, 0.1f, 0.1f);
    spotLight.diffuse = glm::vec3(0.4f, 0.4f, 0.25f);
    spotLight.specular = glm::vec3(0.2f, 0.2f, 0.1f);
    spotLight.constant = 0.5f;
    spotLight.linear = 0.1f;
    spotLight.quadratic = 0.04f;
    spotLight.cutOff = glm::cos(glm::radians(75.0f));
    spotLight.outerCutOff = glm::cos(glm::radians(87.0f));


    hdrShader.use();
    hdrShader.setInt("hdrBuffer", 0);

    // render loop
    // -----------
    while (!glfwWindowShouldClose(window)) {
        // per-frame time logic
        // --------------------
        float currentFrame = glfwGetTime();
        deltaTime = currentFrame - lastFrame;
        lastFrame = currentFrame;

        // input
        // -----
        processInput(window);

        // render into fbo

        glClearColor(0.1f, 0.1f, 0.1f, 1.0f);
        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT | GL_STENCIL_BUFFER_BIT);

        glBindFramebuffer(GL_FRAMEBUFFER, hdrFBO);


        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT | GL_STENCIL_BUFFER_BIT);

        // don't forget to enable shader before setting uniforms
        roomShader.use();

        //pointlight
        if (programState->PointLightEnabled) { // turn on/off pointlight
            exposure = 0.3f;
            roomShader.setVec3("pointLight.ambient", pointLight.ambient);
            roomShader.setVec3("pointLight.diffuse", pointLight.diffuse);
            roomShader.setVec3("pointLight.specular", pointLight.specular);
        } else {
            roomShader.setVec3("pointLight.ambient", glm::vec3(0.0f, 0.0f, 0.0f));
            roomShader.setVec3("pointLight.diffuse", glm::vec3(0.0f, 0.0f, 0.0f));
            roomShader.setVec3("pointLight.specular", glm::vec3(0.0f, 0.0f, 0.0f));
        }
        roomShader.setVec3("pointLight.position", pointLight.position);
        roomShader.setFloat("pointLight.constant", pointLight.constant);
        roomShader.setFloat("pointLight.linear", pointLight.linear);
        roomShader.setFloat("pointLight.quadratic", pointLight.quadratic);

        //dirlight
        roomShader.setVec3("dirLight.direction", dirLight.direction);
        roomShader.setVec3("dirLight.ambient", dirLight.ambient);
        roomShader.setVec3("dirLight.diffuse", dirLight.diffuse);
        roomShader.setVec3("dirLight.specular", dirLight.specular);

        //spotlight
        if (programState->SpotLightEnabled) { //turn on/off spotlight
            exposure = 2.0f;
            roomShader.setVec3("spotLight.ambient", spotLight.ambient);
            roomShader.setVec3("spotLight.diffuse", spotLight.diffuse);
            roomShader.setVec3("spotLight.specular", spotLight.specular);
        } else {
            roomShader.setVec3("spotLight.ambient", glm::vec3(0.0f, 0.0f, 0.0f));
            roomShader.setVec3("spotLight.diffuse", glm::vec3(0.0f, 0.0f, 0.0f));
            roomShader.setVec3("spotLight.specular", glm::vec3(0.0f, 0.0f, 0.0f));
        }
        if(!programState->SpotLightEnabled && !programState->PointLightEnabled)
            exposure = 0.7f;
        if(programState->SpotLightEnabled && programState->PointLightEnabled)
            exposure = 0.2f;

        roomShader.setVec3("spotLight.direction", spotLight.direction);
        roomShader.setVec3("spotLight.position", spotLight.position);
        roomShader.setFloat("spotLight.constant", spotLight.constant);
        roomShader.setFloat("spotLight.linear", spotLight.linear);
        roomShader.setFloat("spotLight.quadratic", spotLight.quadratic);
        roomShader.setFloat("spotLight.cutOff", spotLight.cutOff);
        roomShader.setFloat("spotLight.outerCutOff", spotLight.outerCutOff);

        roomShader.setVec3("viewPosition", programState->camera.Position);
        roomShader.setFloat("material.shininess", 32.0f);

        // view/projection transformations
        glm::mat4 projection = glm::perspective(glm::radians(programState->camera.Zoom),
                                                (float) SCR_WIDTH / (float) SCR_HEIGHT, 0.1f, 100.0f);
        glm::mat4 view = programState->camera.GetViewMatrix();
        roomShader.setMat4("projection", projection);
        roomShader.setMat4("view", view);

        //render floor

        glActiveTexture(GL_TEXTURE0);
        glBindTexture(GL_TEXTURE_2D, floorDiffuseMap);

        glActiveTexture(GL_TEXTURE1);
        glBindTexture(GL_TEXTURE_2D, floorSpecularMap);

        glm::mat4 model = glm::mat4(1.0f);
        model = glm::scale(model, glm::vec3(30.0f, 0.0f, 30.0f));
        roomShader.setMat4("model", model);

        glBindVertexArray(floorVAO);
        glDrawArrays(GL_TRIANGLES, 0, 6);

        //render walls

        glActiveTexture(GL_TEXTURE0);
        glBindTexture(GL_TEXTURE_2D, wallsDiffuseMap);

        glActiveTexture(GL_TEXTURE1);
        glBindTexture(GL_TEXTURE_2D, wallsSpecularMap);

        model = glm::mat4(1.0f);
        model = glm::translate(model, glm::vec3(0.0, 5.0, 0.0));
        model = glm::scale(model, glm::vec3(30.0f, 10.0f, 30.0f));
        roomShader.setMat4("model", model);

        glBindVertexArray(wallsVAO);
        glDrawArrays(GL_TRIANGLES, 0, 24);

        //render table legs

        glActiveTexture(GL_TEXTURE0);
        glBindTexture(GL_TEXTURE_2D, woodDiffuseMap);

        glActiveTexture(GL_TEXTURE1);
        glBindTexture(GL_TEXTURE_2D, woodSpecularMap);

        glBindVertexArray(cubeVAO);

        for (int i = 0; i < 4; i++) {
            model = glm::mat4(1.0f);
            model = glm::translate(model, tableLegsPosition[i]);
            model = glm::scale(model, glm::vec3(0.5f, 2.0f, 0.5f));

            roomShader.setMat4("model", model);
            glDrawArrays(GL_TRIANGLES, 0, 36);
        }

        // render the loaded models

        //cat

        model = glm::mat4(1.0f);
        model = glm::translate(model, glm::vec3(7.0f, 0.0f, 0.0f));
        model = glm::rotate(model, glm::radians(-90.0f), glm::vec3(1.0, 0.0, 0.0));
        model = glm::rotate(model, glm::radians(-45.0f), glm::vec3(0.0, 0.0, 1.0));
        model = glm::scale(model, glm::vec3(0.08f));
        roomShader.setMat4("model", model);
        catModel.Draw(roomShader);


        //lamp
        glActiveTexture(GL_TEXTURE0);
        glBindTexture(GL_TEXTURE_2D, lampTexture);

        model = glm::mat4(1.0f);
        model = glm::translate(model,glm::vec3(0.0f, 8.0f, 0.0f));
        model = glm::rotate(model, glm::radians(-90.0f), glm::vec3(1.0f, 0.0f, 0.0f));
        model = glm::scale(model, glm::vec3(0.03f));
        roomShader.setMat4("model", model);
        lampModel.Draw(roomShader);

        //tv

        glDisable(GL_CULL_FACE); // sofa ana tv not rendering correctly

        model = glm::mat4(1.0f);
        model = glm::translate(model,glm::vec3(-3.0, 3.0, -11.0));

        roomShader.setMat4("model", model);
        tvModel.Draw(roomShader);

        //sofa


        glActiveTexture(GL_TEXTURE0);
        glBindTexture(GL_TEXTURE_2D, sofaDiffuse);

        glActiveTexture(GL_TEXTURE1);
        glBindTexture(GL_TEXTURE_2D, sofaSpecular);

        model = glm::mat4(1.0f);
        model = glm::translate(model,glm::vec3(-5.0f, 0.6f, 9.0f));
        model = glm::scale(model, glm::vec3(0.025f));
        roomShader.setMat4("model", model);
        sofaModel.Draw(roomShader);

        //table glass

        glEnable(GL_BLEND);

        glActiveTexture(GL_TEXTURE0);
        glBindTexture(GL_TEXTURE_2D, glassDiffuseMap);
        glActiveTexture(GL_TEXTURE1);
        glBindTexture(GL_TEXTURE_2D, glassSpecularMap);

        model = glm::mat4(1.0f);
        model = glm::translate(model, glm::vec3(-1.53f, 2.01f, 1.49f));
        model = glm::scale(model, glm::vec3(9.6f, 0.0f, 7.6f));
        roomShader.setMat4("model", model);

        glBindVertexArray(glassVAO);
        glDrawArrays(GL_TRIANGLES, 0, 6);

        glDisable(GL_BLEND);
        glEnable(GL_CULL_FACE); // glass have both sides

        //cubemap

        glDepthFunc(GL_LEQUAL);
        cubemapShader.use();
        view = glm::mat4(glm::mat3(programState->camera.GetViewMatrix()));
        cubemapShader.setMat4("view", view);
        cubemapShader.setMat4("projection", projection);
        // skybox cube
        glBindVertexArray(cubemapVAO);
        glActiveTexture(GL_TEXTURE0);
        glBindTexture(GL_TEXTURE_CUBE_MAP, cubemapTexture);
        glDrawArrays(GL_TRIANGLES, 0, 36);
        glBindVertexArray(0);
        glDepthFunc(GL_LESS);

        if (programState->ImGuiEnabled)
            DrawImGui(programState);

        renderCube();

        glBindFramebuffer(GL_FRAMEBUFFER, 0);

        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT | GL_STENCIL_BUFFER_BIT);
        hdrShader.use();
        glActiveTexture(GL_TEXTURE0);
        glBindTexture(GL_TEXTURE_2D, colorBuffer);
        hdrShader.setInt("hdr", hdr);
        hdrShader.setFloat("exposure", exposure);
        renderQuad();

        std::cout << "hdr: " << (hdr ? "on" : "off") << std::endl;

        // glfw: swap buffers and poll IO events (keys pressed/released, mouse moved etc.)
        // -------------------------------------------------------------------------------
        glfwSwapBuffers(window);
        glfwPollEvents();
    }

    programState->SaveToFile("resources/program_state.txt");
    delete programState;
    ImGui_ImplOpenGL3_Shutdown();
    ImGui_ImplGlfw_Shutdown();
    ImGui::DestroyContext();

    glDeleteVertexArrays(1, &floorVAO);
    glDeleteVertexArrays(1, &wallsVAO);
    glDeleteVertexArrays(1, &cubeVAO);
    glDeleteVertexArrays(1, &glassVAO);
    glDeleteVertexArrays(1, &cubemapVAO);
    glDeleteBuffers(1, &floorVBO);
    glDeleteBuffers(1, &wallsVBO);
    glDeleteBuffers(1, &cubeVBO);
    glDeleteBuffers(1, &glassVBO);
    glDeleteBuffers(1, &cubemapVBO);

    glfwTerminate();
    return 0;
}

// process all input: query GLFW whether relevant keys are pressed/released this frame and react accordingly
// ---------------------------------------------------------------------------------------------------------
void processInput(GLFWwindow *window) {
    if (glfwGetKey(window, GLFW_KEY_ESCAPE) == GLFW_PRESS)
        glfwSetWindowShouldClose(window, true);

    if (glfwGetKey(window, GLFW_KEY_W) == GLFW_PRESS)
        programState->camera.ProcessKeyboard(FORWARD, deltaTime);
    if (glfwGetKey(window, GLFW_KEY_S) == GLFW_PRESS)
        programState->camera.ProcessKeyboard(BACKWARD, deltaTime);
    if (glfwGetKey(window, GLFW_KEY_A) == GLFW_PRESS)
        programState->camera.ProcessKeyboard(LEFT, deltaTime);
    if (glfwGetKey(window, GLFW_KEY_D) == GLFW_PRESS)
        programState->camera.ProcessKeyboard(RIGHT, deltaTime);

    if (glfwGetKey(window, GLFW_KEY_SPACE) == GLFW_PRESS && !hdrKeyPressed)
    {
        hdr = !hdr;
        hdrKeyPressed = true;
    }
    if (glfwGetKey(window, GLFW_KEY_SPACE) == GLFW_RELEASE)
    {
        hdrKeyPressed = false;
    }
}

// glfw: whenever the window size changed (by OS or user resize) this callback function executes
// ---------------------------------------------------------------------------------------------
void framebuffer_size_callback(GLFWwindow *window, int width, int height) {
    // make sure the viewport matches the new window dimensions; note that width and
    // height will be significantly larger than specified on retina displays.
    glViewport(0, 0, width, height);
}

// glfw: whenever the mouse moves, this callback is called
// -------------------------------------------------------
void mouse_callback(GLFWwindow *window, double xpos, double ypos) {
    if (firstMouse) {
        lastX = xpos;
        lastY = ypos;
        firstMouse = false;
    }

    float xoffset = xpos - lastX;
    float yoffset = lastY - ypos; // reversed since y-coordinates go from bottom to top

    lastX = xpos;
    lastY = ypos;

    if (programState->CameraMouseMovementUpdateEnabled)
        programState->camera.ProcessMouseMovement(xoffset, yoffset);
}

// glfw: whenever the mouse scroll wheel scrolls, this callback is called
// ----------------------------------------------------------------------
void scroll_callback(GLFWwindow *window, double xoffset, double yoffset) {
    programState->camera.ProcessMouseScroll(yoffset);
}

void DrawImGui(ProgramState *programState) {
    ImGui_ImplOpenGL3_NewFrame();
    ImGui_ImplGlfw_NewFrame();
    ImGui::NewFrame();


    {
        static float f = 0.0f;
        ImGui::Begin("Light settings");

        ImGui::DragFloat("pointLight.constant", &programState->pointLight.constant, 0.05, 0.0, 1.0);
        ImGui::DragFloat("pointLight.linear", &programState->pointLight.linear, 0.05, 0.0, 1.0);
        ImGui::DragFloat("pointLight.quadratic", &programState->pointLight.quadratic, 0.05, 0.0, 1.0);

        ImGui::DragFloat("spotLight.constant", &programState->spotLight.constant, 0.05, 0.0, 1.0);
        ImGui::DragFloat("spotLight.linear", &programState->spotLight.linear, 0.05, 0.0, 1.0);
        ImGui::DragFloat("spotLight.quadratic", &programState->spotLight.quadratic, 0.05, 0.0, 1.0);

        ImGui::Checkbox("Turn on point light", &programState->PointLightEnabled);
        ImGui::Checkbox("Turn on spot light", &programState->SpotLightEnabled);

        ImGui::End();
    }

    {
        ImGui::Begin("Camera info");
        const Camera& c = programState->camera;
        ImGui::Text("Camera position: (%f, %f, %f)", c.Position.x, c.Position.y, c.Position.z);
        ImGui::Text("(Yaw, Pitch): (%f, %f)", c.Yaw, c.Pitch);
        ImGui::Text("Camera front: (%f, %f, %f)", c.Front.x, c.Front.y, c.Front.z);
        ImGui::Checkbox("Camera mouse update", &programState->CameraMouseMovementUpdateEnabled);
        ImGui::End();
    }

    ImGui::Render();
    ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());
}

void key_callback(GLFWwindow *window, int key, int scancode, int action, int mods) {
    if (key == GLFW_KEY_F1 && action == GLFW_PRESS) {
        programState->ImGuiEnabled = !programState->ImGuiEnabled;
        if (programState->ImGuiEnabled) {
            programState->CameraMouseMovementUpdateEnabled = false;
            glfwSetInputMode(window, GLFW_CURSOR, GLFW_CURSOR_NORMAL);
        } else {
            //glfwSetInputMode(window, GLFW_CURSOR, GLFW_CURSOR_DISABLED);
        }
    }
}
// utility function for loading a 2D texture from file
// ---------------------------------------------------
unsigned int loadTexture(char const * path, bool gammaCorrection)
{
    unsigned int textureID;
    glGenTextures(1, &textureID);

    int width, height, nrComponents;
    unsigned char *data = stbi_load(path, &width, &height, &nrComponents, 0);
    if (data)
    {
        GLenum internalFormat;
        GLenum dataFormat;
        if (nrComponents == 1)
        {
            internalFormat = dataFormat = GL_RED;
        }
        else if (nrComponents == 3)
        {
            internalFormat = gammaCorrection ? GL_SRGB : GL_RGB;
            dataFormat = GL_RGB;
        }
        else if (nrComponents == 4)
        {
            internalFormat = gammaCorrection ? GL_SRGB_ALPHA : GL_RGBA;
            dataFormat = GL_RGBA;
        }

        glBindTexture(GL_TEXTURE_2D, textureID);
        glTexImage2D(GL_TEXTURE_2D, 0, internalFormat, width, height, 0, dataFormat, GL_UNSIGNED_BYTE, data);
        glGenerateMipmap(GL_TEXTURE_2D);

        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR_MIPMAP_LINEAR);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);

        stbi_image_free(data);
    }
    else
    {
        std::cout << "Texture failed to load at path: " << path << std::endl;
        stbi_image_free(data);
    }

    return textureID;
}

// loads a cubemap texture from 6 individual texture faces
// order:
// +X (right)
// -X (left)
// +Y (top)
// -Y (bottom)
// +Z (front)
// -Z (back)
// -------------------------------------------------------
unsigned int loadCubemap(vector<std::string> faces)
{
    unsigned int textureID;
    glGenTextures(1, &textureID);
    glBindTexture(GL_TEXTURE_CUBE_MAP, textureID);

    int width, height, nrChannels;
    for (unsigned int i = 0; i < faces.size(); i++)
    {
        unsigned char *data = stbi_load(faces[i].c_str(), &width, &height, &nrChannels, 0);
        if (data)
        {
            glTexImage2D(GL_TEXTURE_CUBE_MAP_POSITIVE_X + i, 0, GL_RGB, width, height, 0, GL_RGB, GL_UNSIGNED_BYTE, data);
            stbi_image_free(data);
        }
        else
        {
            std::cout << "Cubemap texture failed to load at path: " << faces[i] << std::endl;
            stbi_image_free(data);
        }
    }
    glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_WRAP_R, GL_CLAMP_TO_EDGE);

    return textureID;
}

// renderCube() renders a 1x1 3D cube in NDC.
// -------------------------------------------------
unsigned int cubeVAO = 0;
unsigned int cubeVBO = 0;
void renderCube()
{
    // initialize (if necessary)
    if (cubeVAO == 0)
    {
        float vertices[] = {
                // back face
                -1.0f, -1.0f, -1.0f,  0.0f,  0.0f, -1.0f, 0.0f, 0.0f, // bottom-left
                1.0f,  1.0f, -1.0f,  0.0f,  0.0f, -1.0f, 1.0f, 1.0f, // top-right
                1.0f, -1.0f, -1.0f,  0.0f,  0.0f, -1.0f, 1.0f, 0.0f, // bottom-right
                1.0f,  1.0f, -1.0f,  0.0f,  0.0f, -1.0f, 1.0f, 1.0f, // top-right
                -1.0f, -1.0f, -1.0f,  0.0f,  0.0f, -1.0f, 0.0f, 0.0f, // bottom-left
                -1.0f,  1.0f, -1.0f,  0.0f,  0.0f, -1.0f, 0.0f, 1.0f, // top-left
                // front face
                -1.0f, -1.0f,  1.0f,  0.0f,  0.0f,  1.0f, 0.0f, 0.0f, // bottom-left
                1.0f, -1.0f,  1.0f,  0.0f,  0.0f,  1.0f, 1.0f, 0.0f, // bottom-right
                1.0f,  1.0f,  1.0f,  0.0f,  0.0f,  1.0f, 1.0f, 1.0f, // top-right
                1.0f,  1.0f,  1.0f,  0.0f,  0.0f,  1.0f, 1.0f, 1.0f, // top-right
                -1.0f,  1.0f,  1.0f,  0.0f,  0.0f,  1.0f, 0.0f, 1.0f, // top-left
                -1.0f, -1.0f,  1.0f,  0.0f,  0.0f,  1.0f, 0.0f, 0.0f, // bottom-left
                // left face
                -1.0f,  1.0f,  1.0f, -1.0f,  0.0f,  0.0f, 1.0f, 0.0f, // top-right
                -1.0f,  1.0f, -1.0f, -1.0f,  0.0f,  0.0f, 1.0f, 1.0f, // top-left
                -1.0f, -1.0f, -1.0f, -1.0f,  0.0f,  0.0f, 0.0f, 1.0f, // bottom-left
                -1.0f, -1.0f, -1.0f, -1.0f,  0.0f,  0.0f, 0.0f, 1.0f, // bottom-left
                -1.0f, -1.0f,  1.0f, -1.0f,  0.0f,  0.0f, 0.0f, 0.0f, // bottom-right
                -1.0f,  1.0f,  1.0f, -1.0f,  0.0f,  0.0f, 1.0f, 0.0f, // top-right
                // right face
                1.0f,  1.0f,  1.0f,  1.0f,  0.0f,  0.0f, 1.0f, 0.0f, // top-left
                1.0f, -1.0f, -1.0f,  1.0f,  0.0f,  0.0f, 0.0f, 1.0f, // bottom-right
                1.0f,  1.0f, -1.0f,  1.0f,  0.0f,  0.0f, 1.0f, 1.0f, // top-right
                1.0f, -1.0f, -1.0f,  1.0f,  0.0f,  0.0f, 0.0f, 1.0f, // bottom-right
                1.0f,  1.0f,  1.0f,  1.0f,  0.0f,  0.0f, 1.0f, 0.0f, // top-left
                1.0f, -1.0f,  1.0f,  1.0f,  0.0f,  0.0f, 0.0f, 0.0f, // bottom-left
                // bottom face
                -1.0f, -1.0f, -1.0f,  0.0f, -1.0f,  0.0f, 0.0f, 1.0f, // top-right
                1.0f, -1.0f, -1.0f,  0.0f, -1.0f,  0.0f, 1.0f, 1.0f, // top-left
                1.0f, -1.0f,  1.0f,  0.0f, -1.0f,  0.0f, 1.0f, 0.0f, // bottom-left
                1.0f, -1.0f,  1.0f,  0.0f, -1.0f,  0.0f, 1.0f, 0.0f, // bottom-left
                -1.0f, -1.0f,  1.0f,  0.0f, -1.0f,  0.0f, 0.0f, 0.0f, // bottom-right
                -1.0f, -1.0f, -1.0f,  0.0f, -1.0f,  0.0f, 0.0f, 1.0f, // top-right
                // top face
                -1.0f,  1.0f, -1.0f,  0.0f,  1.0f,  0.0f, 0.0f, 1.0f, // top-left
                1.0f,  1.0f , 1.0f,  0.0f,  1.0f,  0.0f, 1.0f, 0.0f, // bottom-right
                1.0f,  1.0f, -1.0f,  0.0f,  1.0f,  0.0f, 1.0f, 1.0f, // top-right
                1.0f,  1.0f,  1.0f,  0.0f,  1.0f,  0.0f, 1.0f, 0.0f, // bottom-right
                -1.0f,  1.0f, -1.0f,  0.0f,  1.0f,  0.0f, 0.0f, 1.0f, // top-left
                -1.0f,  1.0f,  1.0f,  0.0f,  1.0f,  0.0f, 0.0f, 0.0f  // bottom-left
        };
        glGenVertexArrays(1, &cubeVAO);
        glGenBuffers(1, &cubeVBO);
        // fill buffer
        glBindBuffer(GL_ARRAY_BUFFER, cubeVBO);
        glBufferData(GL_ARRAY_BUFFER, sizeof(vertices), vertices, GL_STATIC_DRAW);
        // link vertex attributes
        glBindVertexArray(cubeVAO);
        glEnableVertexAttribArray(0);
        glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 8 * sizeof(float), (void*)0);
        glEnableVertexAttribArray(1);
        glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, 8 * sizeof(float), (void*)(3 * sizeof(float)));
        glEnableVertexAttribArray(2);
        glVertexAttribPointer(2, 2, GL_FLOAT, GL_FALSE, 8 * sizeof(float), (void*)(6 * sizeof(float)));
        glBindBuffer(GL_ARRAY_BUFFER, 0);
        glBindVertexArray(0);
    }
    // render Cube
    glBindVertexArray(cubeVAO);
    glDrawArrays(GL_TRIANGLES, 0, 36);
    glBindVertexArray(0);
}

// renderQuad() renders a 1x1 XY quad in NDC
// -----------------------------------------
unsigned int quadVAO = 0;
unsigned int quadVBO;
void renderQuad()
{
    if (quadVAO == 0)
    {
        float quadVertices[] = {
                // positions        // texture Coords
                -1.0f,  1.0f, 0.0f, 0.0f, 1.0f,
                -1.0f, -1.0f, 0.0f, 0.0f, 0.0f,
                1.0f,  1.0f, 0.0f, 1.0f, 1.0f,
                1.0f, -1.0f, 0.0f, 1.0f, 0.0f,
        };
        // setup plane VAO
        glGenVertexArrays(1, &quadVAO);
        glGenBuffers(1, &quadVBO);
        glBindVertexArray(quadVAO);
        glBindBuffer(GL_ARRAY_BUFFER, quadVBO);
        glBufferData(GL_ARRAY_BUFFER, sizeof(quadVertices), &quadVertices, GL_STATIC_DRAW);
        glEnableVertexAttribArray(0);
        glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 5 * sizeof(float), (void*)0);
        glEnableVertexAttribArray(1);
        glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, 5 * sizeof(float), (void*)(3 * sizeof(float)));
    }
    glBindVertexArray(quadVAO);
    glDrawArrays(GL_TRIANGLE_STRIP, 0, 4);
    glBindVertexArray(0);
}

