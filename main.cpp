// week11: суретті файлдан жүктеу, текстура объектісі, UV, wrap/filter/mipmap
#define STB_IMAGE_IMPLEMENTATION
#include <stb_image.h>

#include <glad/glad.h>
#include <GLFW/glfw3.h>
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/type_ptr.hpp>

#include <cmath>
#include <cstdio>
#include <iostream>
#include <string>

#ifndef ASSET_DIR
#define ASSET_DIR "assets"
#endif

// Анизотропты сүзгі тұрақтылары (EXT_texture_filter_anisotropic) — glad 3.3-те болмауы мүмкін
#ifndef GL_TEXTURE_MAX_ANISOTROPY
#define GL_TEXTURE_MAX_ANISOTROPY 0x84FE
#endif
#ifndef GL_MAX_TEXTURE_MAX_ANISOTROPY
#define GL_MAX_TEXTURE_MAX_ANISOTROPY 0x84FF
#endif

// ---------------- Камера (9–10 апта) ----------------
const glm::vec3 CAM_START_POS = glm::vec3(0.0f, 0.0f, 3.0f);
glm::vec3 camPos   = CAM_START_POS;
glm::vec3 camFront = glm::vec3(0.0f, 0.0f, -1.0f);
glm::vec3 camUp    = glm::vec3(0.0f, 1.0f, 0.0f);

const float YAW_START = -90.0f;
const float FOV_START = 45.0f;
const float FOV_MAX   = 90.0f;
float yaw   = YAW_START;
float pitch = 0.0f;
float lastX = 400.0f;
float lastY = 300.0f;
bool firstMouse = true;
float fov = FOV_START;
const float sensitivity = 0.1f;
bool invertY = false;

// ---------------- 11-апта күйі ----------------
float uvScale = 1.0f;                 // 5-тапсырма: [ және ]
GLenum wrapMode = GL_REPEAT;          // 1-тапсырма: 1..4
bool magNearest = false;              // 2-тапсырма: N / L
bool minMipmap  = true;               // 3-тапсырма: M (mipmap қосу/өшіру)
bool brokenMag  = false;              // 4-тапсырма: B (MAG-қа mipmap тұрақтысы)
bool noMipTexture = false;            // 4-тапсырма: K (mipmap жасалмаған текстура)
bool useAniso   = false;              // Қ4: X
bool tintOn     = false;              // Қ2: T
bool flowOn     = false;              // Қ3: F
bool showFloor  = false;              // Қ4 үшін еден: G
float maxAniso  = 0.0f;
bool reportMagError = false;

bool keyLast[GLFW_KEY_LAST + 1] = { false };
bool justPressed(GLFWwindow* w, int key) {
    bool now = glfwGetKey(w, key) == GLFW_PRESS;
    bool r = now && !keyLast[key];
    keyLast[key] = now;
    return r;
}

// ---------------- Шейдерлер ----------------
const char* vertexSrc = R"(#version 330 core
layout (location = 0) in vec3 aPos;
layout (location = 1) in vec2 aTexCoord;
out vec2 vTexCoord;
uniform mat4 model;
uniform mat4 view;
uniform mat4 projection;
void main() {
    gl_Position = projection * view * model * vec4(aPos, 1.0);
    vTexCoord = aTexCoord;
}
)";

const char* fragmentSrc = R"(#version 330 core
in vec2 vTexCoord;
out vec4 FragColor;
uniform sampler2D ourTexture;
uniform float uvScale;   // 5-тапсырма
uniform float uvTile;    // әр объектіге жеке қайталау (еден үшін)
uniform float time;      // Қ3
uniform float flow;      // Қ3: 0 немесе 1
uniform vec3 tint;       // Қ2
void main() {
    vec2 uv = vTexCoord * uvScale * uvTile + vec2(0.0, time * 0.1 * flow);
    FragColor = texture(ourTexture, uv) * vec4(tint, 1.0);
}
)";

// 3-қадам: позиция (3) + UV (2). Орау реті (CCW) 10-аптадағыдай.
// Әр жақта UV реті: (0,0) (1,0) (1,1)  (1,1) (0,1) (0,0)
constexpr float s = 0.5f;
float vertices[] = {
    // алдыңғы (+z)
    -s,-s, s, 0,0,    s,-s, s, 1,0,    s, s, s, 1,1,
     s, s, s, 1,1,   -s, s, s, 0,1,   -s,-s, s, 0,0,
    // артқы (-z)
     s,-s,-s, 0,0,   -s,-s,-s, 1,0,   -s, s,-s, 1,1,
    -s, s,-s, 1,1,    s, s,-s, 0,1,    s,-s,-s, 0,0,
    // оң (+x)
     s,-s, s, 0,0,    s,-s,-s, 1,0,    s, s,-s, 1,1,
     s, s,-s, 1,1,    s, s, s, 0,1,    s,-s, s, 0,0,
    // сол (-x)
    -s,-s,-s, 0,0,   -s,-s, s, 1,0,   -s, s, s, 1,1,
    -s, s, s, 1,1,   -s, s,-s, 0,1,   -s,-s,-s, 0,0,
    // жоғарғы (+y)
    -s, s, s, 0,0,    s, s, s, 1,0,    s, s,-s, 1,1,
     s, s,-s, 1,1,   -s, s,-s, 0,1,   -s, s, s, 0,0,
    // төменгі (-y)
    -s,-s,-s, 0,0,    s,-s,-s, 1,0,    s,-s, s, 1,1,
     s,-s, s, 1,1,   -s,-s, s, 0,1,   -s,-s,-s, 0,0,
};

glm::vec3 cubePositions[] = {
    glm::vec3( 0.0f,  0.0f,   0.0f),
    glm::vec3( 1.8f,  0.6f,  -1.0f),
    glm::vec3(-1.8f, -0.6f,  -2.0f),
    glm::vec3( 1.5f, -1.0f,  -3.0f),
    glm::vec3(-1.2f,  1.0f,  -4.0f),
    // 3-тапсырма: z = -40 маңындағы алыс кубтар (mipmap әсерін көру үшін)
    glm::vec3( 0.0f,  0.0f, -40.0f),
    glm::vec3( 2.0f,  1.0f, -41.0f),
    glm::vec3(-2.0f, -1.0f, -42.0f),
    glm::vec3( 3.5f, -0.5f, -39.0f),
    glm::vec3(-3.5f,  0.5f, -43.0f),
    glm::vec3( 1.0f, -1.5f, -44.0f),
};
const int cubeCount = sizeof(cubePositions) / sizeof(cubePositions[0]);

// ---------------- Камера: yaw/pitch ----------------
void updateCameraFront() {
    glm::vec3 dir;
    dir.x = cos(glm::radians(yaw)) * cos(glm::radians(pitch));
    dir.y = sin(glm::radians(pitch));
    dir.z = sin(glm::radians(yaw)) * cos(glm::radians(pitch));
    camFront = glm::normalize(dir);
}

void framebufferSizeCallback(GLFWwindow*, int w, int h) { glViewport(0, 0, w, h); }

void mouse_callback(GLFWwindow*, double xpos, double ypos) {
    if (firstMouse) { lastX = (float)xpos; lastY = (float)ypos; firstMouse = false; }
    float xoffset = (float)xpos - lastX;
    float yoffset = lastY - (float)ypos;
    if (invertY) yoffset = -yoffset;
    lastX = (float)xpos;
    lastY = (float)ypos;
    yaw   += xoffset * sensitivity;
    pitch += yoffset * sensitivity;
    if (yaw > 360.0f)  yaw -= 360.0f;
    if (yaw < -360.0f) yaw += 360.0f;
    if (pitch > 89.0f) pitch = 89.0f;
    if (pitch < -89.0f) pitch = -89.0f;
    updateCameraFront();
}

void scroll_callback(GLFWwindow*, double, double yoffset) {
    fov -= (float)yoffset;
    if (fov < 1.0f) fov = 1.0f;
    if (fov > FOV_MAX) fov = FOV_MAX;
}

const char* wrapName(GLenum m) {
    switch (m) {
        case GL_REPEAT: return "REPEAT";
        case GL_MIRRORED_REPEAT: return "MIRRORED_REPEAT";
        case GL_CLAMP_TO_EDGE: return "CLAMP_TO_EDGE";
        case GL_CLAMP_TO_BORDER: return "CLAMP_TO_BORDER";
    }
    return "?";
}

void processInput(GLFWwindow* window, float dt) {
    if (glfwGetKey(window, GLFW_KEY_ESCAPE) == GLFW_PRESS) glfwSetWindowShouldClose(window, true);

    float speed = 2.5f * dt;
    if (glfwGetKey(window, GLFW_KEY_LEFT_CONTROL) == GLFW_PRESS) speed *= 2.0f;
    if (glfwGetKey(window, GLFW_KEY_W) == GLFW_PRESS) camPos += speed * camFront;
    if (glfwGetKey(window, GLFW_KEY_S) == GLFW_PRESS) camPos -= speed * camFront;
    if (glfwGetKey(window, GLFW_KEY_A) == GLFW_PRESS) camPos -= glm::normalize(glm::cross(camFront, camUp)) * speed;
    if (glfwGetKey(window, GLFW_KEY_D) == GLFW_PRESS) camPos += glm::normalize(glm::cross(camFront, camUp)) * speed;
    if (glfwGetKey(window, GLFW_KEY_SPACE) == GLFW_PRESS)      camPos += speed * camUp;
    if (glfwGetKey(window, GLFW_KEY_LEFT_SHIFT) == GLFW_PRESS) camPos -= speed * camUp;

    if (justPressed(window, GLFW_KEY_I)) invertY = !invertY;
    if (justPressed(window, GLFW_KEY_P)) {
        std::printf("camPos = (%.2f, %.2f, %.2f)\n", camPos.x, camPos.y, camPos.z);
        std::printf("camFront = (%.2f, %.2f, %.2f)\n", camFront.x, camFront.y, camFront.z);
        std::printf("yaw = %.1f pitch = %.1f fov = %.1f uvScale = %.2f\n", yaw, pitch, fov, uvScale);
    }
    if (justPressed(window, GLFW_KEY_R)) {
        camPos = CAM_START_POS; yaw = YAW_START; pitch = 0.0f; fov = FOV_START; uvScale = 1.0f;
        updateCameraFront();
    }

    // 5-тапсырма: [ және ] — үздіксіз әрекет, сондықтан dt-ге көбейтіледі
    if (glfwGetKey(window, GLFW_KEY_RIGHT_BRACKET) == GLFW_PRESS) uvScale += 2.0f * dt;
    if (glfwGetKey(window, GLFW_KEY_LEFT_BRACKET) == GLFW_PRESS)  uvScale -= 2.0f * dt;
    if (uvScale < 0.1f) uvScale = 0.1f;   // 0.1 — 2-тапсырмадағы «үлкейту» эксперименті үшін
    if (uvScale > 8.0f) uvScale = 8.0f;

    // 1-тапсырма: wrap режимдері
    if (justPressed(window, GLFW_KEY_1)) wrapMode = GL_REPEAT;
    if (justPressed(window, GLFW_KEY_2)) wrapMode = GL_MIRRORED_REPEAT;
    if (justPressed(window, GLFW_KEY_3)) wrapMode = GL_CLAMP_TO_EDGE;
    if (justPressed(window, GLFW_KEY_4)) wrapMode = GL_CLAMP_TO_BORDER;

    // 2-тапсырма: MAG сүзгісі
    if (justPressed(window, GLFW_KEY_N)) { magNearest = true;  brokenMag = false; }
    if (justPressed(window, GLFW_KEY_L)) { magNearest = false; brokenMag = false; }

    // 3-тапсырма: MIN сүзгісі
    if (justPressed(window, GLFW_KEY_M)) minMipmap = !minMipmap;

    // 4-тапсырма: әдейі қателер
    if (justPressed(window, GLFW_KEY_B)) { brokenMag = !brokenMag; reportMagError = brokenMag; }
    if (justPressed(window, GLFW_KEY_K)) noMipTexture = !noMipTexture;

    // Қосымша
    if (justPressed(window, GLFW_KEY_T)) tintOn = !tintOn;
    if (justPressed(window, GLFW_KEY_F)) flowOn = !flowOn;
    if (justPressed(window, GLFW_KEY_X)) useAniso = !useAniso;
    if (justPressed(window, GLFW_KEY_G)) showFloor = !showFloor;

    if (glm::length(camPos) > 60.0f) camPos = glm::normalize(camPos) * 60.0f;
}

GLuint compileShader(GLenum type, const char* src) {
    GLuint sh = glCreateShader(type);
    glShaderSource(sh, 1, &src, nullptr);
    glCompileShader(sh);
    GLint ok; glGetShaderiv(sh, GL_COMPILE_STATUS, &ok);
    if (!ok) { char log[512]; glGetShaderInfoLog(sh, 512, nullptr, log); std::cerr << "Shader error: " << log << "\n"; }
    return sh;
}

// 6-қадам: текстураны жүктеу. withMipmaps=false — 4-тапсырмадағы қара куб үшін
unsigned int loadTexture(const char* path, bool withMipmaps = true) {
    unsigned int id;
    glGenTextures(1, &id);
    glBindTexture(GL_TEXTURE_2D, id);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR_MIPMAP_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);

    stbi_set_flip_vertically_on_load(true);
    int w, h, n;
    unsigned char* data = stbi_load(path, &w, &h, &n, 0);
    if (data) {
        GLenum fmt = (n == 4) ? GL_RGBA : (n == 3) ? GL_RGB : GL_RED;
        glPixelStorei(GL_UNPACK_ALIGNMENT, 1);   // ені 4-ке еселенбейтін суреттер үшін
        glTexImage2D(GL_TEXTURE_2D, 0, fmt, w, h, 0, fmt, GL_UNSIGNED_BYTE, data);
        if (withMipmaps) glGenerateMipmap(GL_TEXTURE_2D);   // glTexImage2D-ден кейін!
        std::cout << path << " " << w << "x" << h << " арна: " << n << std::endl;
    } else {
        std::cerr << "Сурет жүктелмеді: " << path << std::endl;
    }
    stbi_image_free(data);
    return id;
}

// Параметрлерді ағымдағы күйге қарай қою (1–4, Қ4 тапсырмалары)
void applyTexParams(unsigned int tex) {
    glBindTexture(GL_TEXTURE_2D, tex);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, wrapMode);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, wrapMode);
    if (wrapMode == GL_CLAMP_TO_BORDER) {
        float border[] = { 1.0f, 0.0f, 0.0f, 1.0f };   // қызыл
        glTexParameterfv(GL_TEXTURE_2D, GL_TEXTURE_BORDER_COLOR, border);
    }
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, minMipmap ? GL_LINEAR_MIPMAP_LINEAR : GL_LINEAR);

    GLenum mag = magNearest ? GL_NEAREST : GL_LINEAR;
    if (brokenMag) mag = GL_LINEAR_MIPMAP_LINEAR;   // әдейі қате: MAG-қа mipmap тұрақтысы болмайды
    while (glGetError() != GL_NO_ERROR) {}           // ескі қателерді тазалау
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, mag);
    GLenum err = glGetError();
    if (brokenMag && reportMagError) {
        std::printf("MAG = GL_LINEAR_MIPMAP_LINEAR қойылды. glGetError() = 0x%04X %s\n", err,
                    err == GL_INVALID_ENUM ? "(GL_INVALID_ENUM)" : "");
    }

    if (maxAniso > 0.0f)
        glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_MAX_ANISOTROPY, useAniso ? maxAniso : 1.0f);
}

int main() {
    if (!glfwInit()) { std::cerr << "GLFW init failed\n"; return -1; }
    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 3);
    glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);
#ifdef __APPLE__
    glfwWindowHint(GLFW_OPENGL_FORWARD_COMPAT, GL_TRUE);
#endif

    GLFWwindow* window = glfwCreateWindow(800, 600, "week11: textures", nullptr, nullptr);
    if (!window) { std::cerr << "Window failed\n"; glfwTerminate(); return -1; }
    glfwMakeContextCurrent(window);
    glfwSetFramebufferSizeCallback(window, framebufferSizeCallback);
    if (!gladLoadGLLoader((GLADloadproc)glfwGetProcAddress)) { std::cerr << "GLAD failed\n"; return -1; }

    glfwSetInputMode(window, GLFW_CURSOR, GLFW_CURSOR_DISABLED);
    glfwSetCursorPosCallback(window, mouse_callback);
    glfwSetScrollCallback(window, scroll_callback);

    glEnable(GL_DEPTH_TEST);
    glEnable(GL_CULL_FACE);
    glCullFace(GL_BACK);

    GLuint vs = compileShader(GL_VERTEX_SHADER, vertexSrc);
    GLuint fs = compileShader(GL_FRAGMENT_SHADER, fragmentSrc);
    GLuint shader = glCreateProgram();
    glAttachShader(shader, vs); glAttachShader(shader, fs); glLinkProgram(shader);
    glDeleteShader(vs); glDeleteShader(fs);

    GLuint VAO, VBO;
    glGenVertexArrays(1, &VAO); glGenBuffers(1, &VBO);
    glBindVertexArray(VAO);
    glBindBuffer(GL_ARRAY_BUFFER, VBO);
    glBufferData(GL_ARRAY_BUFFER, sizeof(vertices), vertices, GL_STATIC_DRAW);
    // 4-қадам: поз — 3 компонент, UV — 2 компонент, stride 5 (екі жерде де!)
    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 5 * sizeof(float), (void*)0);
    glEnableVertexAttribArray(0);
    glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, 5 * sizeof(float), (void*)(3 * sizeof(float)));
    glEnableVertexAttribArray(1);

    std::string path = std::string(ASSET_DIR) + "/container.png";
    unsigned int texture      = loadTexture(path.c_str(), true);
    unsigned int textureNoMip = loadTexture(path.c_str(), false);

    glGetError();
    glGetFloatv(GL_MAX_TEXTURE_MAX_ANISOTROPY, &maxAniso);
    if (glGetError() != GL_NO_ERROR) maxAniso = 0.0f;

    GLint modelLoc = glGetUniformLocation(shader, "model");
    GLint viewLoc  = glGetUniformLocation(shader, "view");
    GLint projLoc  = glGetUniformLocation(shader, "projection");
    GLint uvLoc    = glGetUniformLocation(shader, "uvScale");
    GLint tileLoc  = glGetUniformLocation(shader, "uvTile");
    GLint timeLoc  = glGetUniformLocation(shader, "time");
    GLint flowLoc  = glGetUniformLocation(shader, "flow");
    GLint tintLoc  = glGetUniformLocation(shader, "tint");

    updateCameraFront();
    float lastTime = (float)glfwGetTime();

    while (!glfwWindowShouldClose(window)) {
        float now = (float)glfwGetTime();
        float dt = now - lastTime;
        lastTime = now;

        glfwPollEvents();
        processInput(window, dt);

        char buf[256];
        std::snprintf(buf, sizeof(buf), "wrap=%s mag=%s min=%s uvScale=%.2f fov=%.0f%s%s",
                      wrapName(wrapMode), brokenMag ? "BROKEN" : (magNearest ? "NEAREST" : "LINEAR"),
                      minMipmap ? "MIPMAP" : "LINEAR", uvScale, fov,
                      noMipTexture ? " [no-mip texture]" : "", useAniso ? " [aniso]" : "");
        glfwSetWindowTitle(window, buf);

        glClearColor(0.1f, 0.1f, 0.15f, 1.0f);
        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
        glUseProgram(shader);

        applyTexParams(texture);
        applyTexParams(textureNoMip);
        reportMagError = false;

        int gWidth, gHeight; glfwGetFramebufferSize(window, &gWidth, &gHeight);
        float aspect = gHeight > 0 ? (float)gWidth / (float)gHeight : 1.0f;
        glm::mat4 projection = glm::perspective(glm::radians(fov), aspect, 0.1f, 100.0f);
        glm::mat4 view = glm::lookAt(camPos, camPos + camFront, camUp);
        glUniformMatrix4fv(viewLoc, 1, GL_FALSE, glm::value_ptr(view));
        glUniformMatrix4fv(projLoc, 1, GL_FALSE, glm::value_ptr(projection));

        // 7-қадам: текстураны байлау
        glUniform1i(glGetUniformLocation(shader, "ourTexture"), 0);
        glActiveTexture(GL_TEXTURE0);
        glBindTexture(GL_TEXTURE_2D, noMipTexture ? textureNoMip : texture);

        float t = (float)glfwGetTime();
        glm::vec3 tint = tintOn ? glm::vec3(0.6f + 0.4f * sin(t), 0.6f + 0.4f * sin(t + 2.0f), 0.6f + 0.4f * sin(t + 4.0f))
                                : glm::vec3(1.0f);
        glUniform1f(uvLoc, uvScale);
        glUniform1f(timeLoc, t);
        glUniform1f(flowLoc, flowOn ? 1.0f : 0.0f);
        glUniform3fv(tintLoc, 1, glm::value_ptr(tint));

        glBindVertexArray(VAO);
        glUniform1f(tileLoc, 1.0f);
        for (int i = 0; i < cubeCount; i++) {
            glm::mat4 model = glm::translate(glm::mat4(1.0f), cubePositions[i]);
            glUniformMatrix4fv(modelLoc, 1, GL_FALSE, glm::value_ptr(model));
            glDrawArrays(GL_TRIANGLES, 0, 36);
        }

        if (showFloor) {   // Қ4: үлкен жалпақ «еден» — қиғаш қарап анизотропияны көру үшін
            glm::mat4 model = glm::translate(glm::mat4(1.0f), glm::vec3(0.0f, -2.5f, -10.0f));
            model = glm::scale(model, glm::vec3(40.0f, 0.2f, 40.0f));
            glUniformMatrix4fv(modelLoc, 1, GL_FALSE, glm::value_ptr(model));
            glUniform1f(tileLoc, 20.0f);
            glDrawArrays(GL_TRIANGLES, 0, 36);
        }

        glfwSwapBuffers(window);
    }

    glDeleteVertexArrays(1, &VAO);
    glDeleteBuffers(1, &VBO);
    glDeleteTextures(1, &texture);
    glDeleteTextures(1, &textureNoMip);
    glDeleteProgram(shader);
    glfwTerminate();
    return 0;
}
