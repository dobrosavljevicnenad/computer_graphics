#include "imgui.h"
#include "imgui_impl_glfw.h"
#include "imgui_impl_opengl3.h"

#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/type_ptr.hpp>

#include <learnopengl/camera.h>
#include <learnopengl/filesystem.h>
#include <learnopengl/model.h>
#include <learnopengl/shader.h>

#include <iostream>

#include <GLFW/glfw3.h>
#include <glad/glad.h>

void framebuffer_size_callback(GLFWwindow *window, int width, int height);

void mouse_callback(GLFWwindow *window, double xpos, double ypos);

void scroll_callback(GLFWwindow *window, double xoffset, double yoffset);

void processInput(GLFWwindow *window);

void key_callback(GLFWwindow *window, int key, int scancode, int action,
		  int mods);

unsigned int loadCubemap(vector<std::string> faces);

void renderQuad();

bool hdr = true;
bool hdrKeyPressed = false;
bool bloom = false;
bool bloomKeyPressed = false;
int increaseSpeed = 1.0f;
float exposure = 1.0f;

// settings
const unsigned int SCR_WIDTH = 1200;
const unsigned int SCR_HEIGHT = 900;

// camera
float lastX = SCR_WIDTH / 2.0f;
float lastY = SCR_HEIGHT / 2.0f;
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

struct ProgramState {
    glm::vec3 clearColor = glm::vec3(0);
    bool ImGuiEnabled = false;
    Camera camera;
    bool CameraMouseMovementUpdateEnabled = true;
    glm::vec3 island1Position = glm::vec3(1.0f);
    glm::vec3 island2Position = glm::vec3(1.0f);
    glm::vec3 island3Position = glm::vec3(1.0f);
    float island1Scale = 1.0f;
    float island2Scale = 1.0f;
    float island3Scale = 1.0f;

    PointLight pointLight;
    ProgramState() : camera(glm::vec3(0.0f, 0.0f, 3.0f)) {}

    void SaveToFile(std::string filename);

    void LoadFromFile(std::string filename);
};

void ProgramState::SaveToFile(std::string filename)
{
    std::ofstream out(filename);
    out << clearColor.r << '\n'
	<< clearColor.g << '\n'
	<< clearColor.b << '\n'
	<< ImGuiEnabled << '\n'
	<< camera.Position.x << '\n'
	<< camera.Position.y << '\n'
	<< camera.Position.z << '\n'
	<< camera.Front.x << '\n'
	<< camera.Front.y << '\n'
	<< camera.Front.z << '\n';
}

void ProgramState::LoadFromFile(std::string filename)
{
    std::ifstream in(filename);
    if (in) {
	in >> clearColor.r >> clearColor.g >> clearColor.b >> ImGuiEnabled >>
	    camera.Position.x >> camera.Position.y >> camera.Position.z >>
	    camera.Front.x >> camera.Front.y >> camera.Front.z;
    }
}

ProgramState *programState;

void DrawImGui(ProgramState *programState);

int main()
{
    // glfw: initialize and configure
    glfwInit();
    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 3);
    glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);

#ifdef __APPLE__
    glfwWindowHint(GLFW_OPENGL_FORWARD_COMPAT, GL_TRUE);
#endif

    // glfw window creation
    GLFWwindow *window =
	glfwCreateWindow(SCR_WIDTH, SCR_HEIGHT, "LearnOpenGL", nullptr, nullptr);
    if (window == nullptr) {
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
    glfwSetInputMode(window, GLFW_CURSOR, GLFW_CURSOR_DISABLED);

    // glad: load all OpenGL function pointers
    if (!gladLoadGLLoader((GLADloadproc)glfwGetProcAddress)) {
	std::cout << "Failed to initialize GLAD" << std::endl;
	return -1;
    }

    // tell stb_image.h to flip loaded texture's on the y-axis (before loading
    // model).
    stbi_set_flip_vertically_on_load(true);

    programState = new ProgramState;
    programState->LoadFromFile("resources/program_state.txt");
    if (programState->ImGuiEnabled) {
	glfwSetInputMode(window, GLFW_CURSOR, GLFW_CURSOR_NORMAL);
    }

    // Init Imgui
    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    ImGuiIO &io = ImGui::GetIO();
    (void)io;

    ImGui_ImplGlfw_InitForOpenGL(window, true);
    ImGui_ImplOpenGL3_Init("#version 330 core");

    // configure global opengl state
    glEnable(GL_DEPTH_TEST);

    // build and compile shaders
    Shader ourShader("resources/shaders/2.model_lighting.vs",
		     "resources/shaders/2.model_lighting.fs");

    Shader skyboxShader("resources/shaders/skybox.vs",
			"resources/shaders/skybox.fs");

    Shader hdrShader("resources/shaders/hdr.vs", "resources/shaders/hdr.fs");

    Shader bloomShader("resources/shaders/bloom.vs",
		       "resources/shaders/bloom.fs");

    unsigned int hdrFBO;
    glGenFramebuffers(1, &hdrFBO);
    glBindFramebuffer(GL_FRAMEBUFFER, hdrFBO);

    unsigned int colorBuffers[2];
    glGenTextures(2, colorBuffers);
    for (unsigned int i = 0; i < 2; i++) {
	glBindTexture(GL_TEXTURE_2D, colorBuffers[i]);
	glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA16F, SCR_WIDTH, SCR_HEIGHT, 0,
		     GL_RGBA, GL_FLOAT, nullptr);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
	// attach texture to framebuffer
	glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0 + i,
			       GL_TEXTURE_2D, colorBuffers[i], 0);
    }

    unsigned int rboDepth;
    glGenRenderbuffers(1, &rboDepth);
    glBindRenderbuffer(GL_RENDERBUFFER, rboDepth);
    glRenderbufferStorage(GL_RENDERBUFFER, GL_DEPTH_COMPONENT, SCR_WIDTH,
			  SCR_HEIGHT);
    glFramebufferRenderbuffer(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT,
			      GL_RENDERBUFFER, rboDepth);
    unsigned int attachments[2] = {GL_COLOR_ATTACHMENT0, GL_COLOR_ATTACHMENT1};
    glDrawBuffers(2, attachments);
    if (glCheckFramebufferStatus(GL_FRAMEBUFFER) != GL_FRAMEBUFFER_COMPLETE)
	std::cout << "Framebuffer not complete!" << std::endl;
    glBindFramebuffer(GL_FRAMEBUFFER, 0);

    // ping-pong-framebuffer for blurring
    unsigned int pingpongFBO[2];
    unsigned int pingpongColorbuffers[2];
    glGenFramebuffers(2, pingpongFBO);
    glGenTextures(2, pingpongColorbuffers);
    for (unsigned int i = 0; i < 2; i++) {
	glBindFramebuffer(GL_FRAMEBUFFER, pingpongFBO[i]);
	glBindTexture(GL_TEXTURE_2D, pingpongColorbuffers[i]);
	glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA16F, SCR_WIDTH, SCR_HEIGHT, 0,
		     GL_RGBA, GL_FLOAT, nullptr);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
	glTexParameteri(
	    GL_TEXTURE_2D, GL_TEXTURE_WRAP_S,
	    GL_CLAMP_TO_EDGE); // we clamp to the edge as the blur filter would
			       // otherwise sample repeated texture values!
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
	glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0,
			       GL_TEXTURE_2D, pingpongColorbuffers[i], 0);
	if (glCheckFramebufferStatus(GL_FRAMEBUFFER) != GL_FRAMEBUFFER_COMPLETE)
	    std::cout << "Framebuffer not complete!" << std::endl;
    }

    // load models
    Model island1("resources/objects/island/untitled.obj");
    island1.SetShaderTextureNamePrefix("material.");
    Model island2("resources/objects/island/untitled.obj");
    island2.SetShaderTextureNamePrefix("material.");
    Model island3("resources/objects/island/untitled.obj");
    island3.SetShaderTextureNamePrefix("material.");

    PointLight &pointLight = programState->pointLight;
    pointLight.position = glm::vec3(4.0f, 4.0, 0.0);
    pointLight.ambient = glm::vec3(0.1, 0.1, 0.1);
    pointLight.diffuse = glm::vec3(0.6, 0.6, 0.6);
    pointLight.specular = glm::vec3(1.0, 1.0, 1.0);

    pointLight.constant = 1.0f;
    pointLight.linear = 0.02f;
    pointLight.quadratic = 0.032f;

    float skyboxVertices[] = {
	// aPos
	-1.0f, 1.0f,  -1.0f, -1.0f, -1.0f, -1.0f, 1.0f,	 -1.0f, -1.0f,
	1.0f,  -1.0f, -1.0f, 1.0f,  1.0f,  -1.0f, -1.0f, 1.0f,	-1.0f,

	-1.0f, -1.0f, 1.0f,  -1.0f, -1.0f, -1.0f, -1.0f, 1.0f,	-1.0f,
	-1.0f, 1.0f,  -1.0f, -1.0f, 1.0f,  1.0f,  -1.0f, -1.0f, 1.0f,

	1.0f,  -1.0f, -1.0f, 1.0f,  -1.0f, 1.0f,  1.0f,	 1.0f,	1.0f,
	1.0f,  1.0f,  1.0f,  1.0f,  1.0f,  -1.0f, 1.0f,	 -1.0f, -1.0f,

	-1.0f, -1.0f, 1.0f,  -1.0f, 1.0f,  1.0f,  1.0f,	 1.0f,	1.0f,
	1.0f,  1.0f,  1.0f,  1.0f,  -1.0f, 1.0f,  -1.0f, -1.0f, 1.0f,

	-1.0f, 1.0f,  -1.0f, 1.0f,  1.0f,  -1.0f, 1.0f,	 1.0f,	1.0f,
	1.0f,  1.0f,  1.0f,  -1.0f, 1.0f,  1.0f,  -1.0f, 1.0f,	-1.0f,

	-1.0f, -1.0f, -1.0f, -1.0f, -1.0f, 1.0f,  1.0f,	 -1.0f, -1.0f,
	1.0f,  -1.0f, -1.0f, -1.0f, -1.0f, 1.0f,  1.0f,	 -1.0f, 1.0f};

    unsigned int skyboxVAO, skyboxVBO;
    glGenVertexArrays(1, &skyboxVAO);
    glGenBuffers(1, &skyboxVBO);
    glBindVertexArray(skyboxVAO);
    glBindBuffer(GL_ARRAY_BUFFER, skyboxVBO);
    glBufferData(GL_ARRAY_BUFFER, sizeof(skyboxVertices), &skyboxVertices,
		 GL_STATIC_DRAW);
    glEnableVertexAttribArray(0);
    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 3 * sizeof(float),
			  (void *)nullptr);

    vector<std::string> faces{
	FileSystem::getPath("resources/textures/skybox/front.jpg"),
	FileSystem::getPath("resources/textures/skybox/back.jpg"),
	FileSystem::getPath("resources/textures/skybox/top.jpg"),
	FileSystem::getPath("resources/textures/skybox/bottom.jpg"),
	FileSystem::getPath("resources/textures/skybox/left.jpg"),
	FileSystem::getPath("resources/textures/skybox/right.jpg")};
    stbi_set_flip_vertically_on_load(true);

    unsigned int cubemapTexture = loadCubemap(faces);

    // configure shaders
    ourShader.use();

    skyboxShader.use();
    skyboxShader.setInt("skybox", 0);

    bloomShader.use();
    bloomShader.setInt("image", 0);

    hdrShader.use();
    hdrShader.setInt("hdrBuffer", 0);
    hdrShader.setInt("bloomBlur", 1);

    while (!glfwWindowShouldClose(window)) {
	// per-frame time logic
	float currentFrame = glfwGetTime();
	deltaTime = currentFrame - lastFrame;
	lastFrame = currentFrame;

	// input
	processInput(window);

	// render
	glClearColor(programState->clearColor.r, programState->clearColor.g,
		     programState->clearColor.b, 1.0f);
	glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

	// don't forget to enable shader before setting uniforms
	ourShader.use();

	glBindFramebuffer(GL_FRAMEBUFFER, hdrFBO);
	glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

	// Directional Lignt
	ourShader.setVec3("dirLight.direction", -6.6f, -25.0f, -6.6f);
	ourShader.setVec3("dirLight.ambient", 0.06, 0.06, 0.06);
	ourShader.setVec3("dirLight.diffuse", 0.6f, 0.2f, 0.2);
	ourShader.setVec3("dirLight.specular", 0.1, 0.1, 0.1);

	// Pointlights
	ourShader.setVec3("pointLight[0].position",
			  glm::vec3(0.00f, 25, -40.00f));
	ourShader.setVec3("pointLight[0].ambient", glm::vec3(0.02, 0.02, 0.02));
	ourShader.setVec3("pointLight[0].diffuse",
			  glm::vec3(0.02, 0.02f, 0.02f));
	ourShader.setVec3("pointLight[0].specular",
			  glm::vec3(0.22, 0.22, 0.22)); // moze malo, fazon 0.22
	ourShader.setFloat("pointLight[0].constant", pointLight.constant);
	ourShader.setFloat("pointLight[0].linear", pointLight.linear);
	ourShader.setFloat("pointLight[0].quadratic", pointLight.quadratic);

	ourShader.setVec3("pointLight[1].position",
			  glm::vec3(30, 30 + 2 * sin(glfwGetTime() * 2), -1));
	ourShader.setVec3("pointLight[1].ambient",
			  glm::vec3(0.003, 0.003, 0.003));
	ourShader.setVec3("pointLight[1].diffuse", glm::vec3(1.55, 1.55, 1.56));
	ourShader.setVec3("pointLight[1].specular",
			  glm::vec3(1.12, 1.12, 1.12));
	ourShader.setFloat("pointLight[1].constant", pointLight.constant);
	ourShader.setFloat("pointLight[1].linear", pointLight.linear);
	ourShader.setFloat("pointLight[1].quadratic", pointLight.quadratic);

	ourShader.setVec3("pointLight[2].position", glm::vec3(-40, 25, -20));
	ourShader.setVec3("pointLight[2].ambient", glm::vec3(0.04, 0.04, 0.04));
	ourShader.setVec3("pointLight[2].diffuse", glm::vec3(0.2, 0.2, 0.2));
	ourShader.setVec3("pointLight[2].specular",
			  glm::vec3(0.22, 0.22, 0.22));
	ourShader.setFloat("pointLight[2].constant", pointLight.constant);
	ourShader.setFloat("pointLight[2].linear", pointLight.linear);
	ourShader.setFloat("pointLight[2].quadratic", pointLight.quadratic);

	ourShader.setVec3("pointLight[3].position",
			  glm::vec3(0.00f, 25, -40.00f));
	ourShader.setVec3("pointLight[3].ambient", glm::vec3(0.04, 0.04, 0.04));
	ourShader.setVec3("pointLight[3].diffuse", glm::vec3(0.2, 0.2f, 0.2f));
	ourShader.setVec3("pointLight[3].specular",
			  glm::vec3(0.22, 0.22, 0.22));
	ourShader.setFloat("pointLight[3].constant", pointLight.constant);
	ourShader.setFloat("pointLight[3].linear", pointLight.linear);
	ourShader.setFloat("pointLight[3].quadratic", pointLight.quadratic);

	ourShader.setVec3("viewPosition", programState->camera.Position);
	ourShader.setFloat("material.shininess", 32.0f);

	// view/projection transformations
	glm::mat4 projection = glm::perspective(
	    glm::radians(programState->camera.Zoom),
	    (float)SCR_WIDTH / (float)SCR_HEIGHT, 0.1f, 100.0f);
	glm::mat4 view = programState->camera.GetViewMatrix();
	ourShader.setMat4("projection", projection);
	ourShader.setMat4("view", view);

	glm::mat4 model = glm::mat4(1.0f);
	model = glm::translate(
	    model, glm::vec3(0.00f, 17.00f + 2 * sin(glfwGetTime()), -40.00f));
	model = glm::scale(
	    model,
	    glm::vec3(
		0.02, 0.02,
		0.02)); // it's a bit too big for our scene, so scale it down
	model = glm::rotate(model, glm::radians(-55.0f),
			    glm::vec3(0.0f, 1.0f, 0.0f));
	ourShader.setMat4("model", model);
	island1.Draw(ourShader);

	model = glm::mat4(1.0f);
	model = glm::translate(
	    model, glm::vec3(20.0f, 17.00f + 2 * sin(glfwGetTime()), -0.00f));
	model = glm::scale(model, glm::vec3(0.02, 0.02, 0.02));
	model = glm::rotate(model, glm::radians(-130.0f),
			    glm::vec3(0.0f, 1.0f, 0.0f));
	ourShader.setMat4("model", model);
	island2.Draw(ourShader);

	model = glm::mat4(1.0f);
	model = glm::translate(
	    model, glm::vec3(-40.0f, 17.00f + 2 * sin(glfwGetTime()), -20.00f));
	model = glm::scale(model, glm::vec3(0.02, 0.02, 0.02));
	model = glm::rotate(model, glm::radians(20.0f),
			    glm::vec3(0.0f, 1.0f, 0.0f));
	ourShader.setMat4("model", model);
	island3.Draw(ourShader);

	// skybox always goes last
	glDepthFunc(GL_LEQUAL);
	skyboxShader.use();
	model = glm::mat4(1.0f);
	projection = glm::perspective(glm::radians(programState->camera.Zoom),
				      (float)SCR_WIDTH / (float)SCR_HEIGHT,
				      0.1f, 100.0f);
	view = glm::mat4(glm::mat3(programState->camera.GetViewMatrix()));
	skyboxShader.setMat4("view", view);
	skyboxShader.setMat4("projection", projection);

	// skybox cube
	glBindVertexArray(skyboxVAO);
	glActiveTexture(GL_TEXTURE0);
	glBindTexture(GL_TEXTURE_CUBE_MAP, cubemapTexture);
	glDrawArrays(GL_TRIANGLES, 0, 36);
	glBindVertexArray(0);
	glDepthFunc(GL_LESS); // depth function back to normal state.

	// loading pingpong
	bool horizontal = true, first_iteration = true;
	unsigned int amount = 10;
	bloomShader.use();
	for (unsigned int i = 0; i < amount; i++) {
	    glBindFramebuffer(GL_FRAMEBUFFER, pingpongFBO[horizontal]);
	    bloomShader.setInt("horizontal", horizontal);
	    glBindTexture(GL_TEXTURE_2D,
			  first_iteration ? colorBuffers[1]
					  : pingpongColorbuffers[!horizontal]);

	    renderQuad();

	    horizontal = !horizontal;
	    if (first_iteration)
		first_iteration = false;
	}
	glBindFramebuffer(GL_FRAMEBUFFER, 0);

	// hdr/bloom
	glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
	hdrShader.use();
	glActiveTexture(GL_TEXTURE0);
	glBindTexture(GL_TEXTURE_2D, colorBuffers[0]);
	glActiveTexture(GL_TEXTURE1);
	glBindTexture(GL_TEXTURE_2D, pingpongColorbuffers[!horizontal]);
	hdrShader.setBool("hdr", hdr);
	hdrShader.setBool("bloom", bloom);
	hdrShader.setFloat("exposure", exposure);
	renderQuad();

	if (programState->ImGuiEnabled)
	    DrawImGui(programState);

	// glfw: swap buffers and poll IO events (keys pressed/released, mouse
	// moved etc.)
	glfwSwapBuffers(window);
	glfwPollEvents();
    }

    programState->SaveToFile("resources/program_state.txt");
    delete programState;
    ImGui_ImplOpenGL3_Shutdown();
    ImGui_ImplGlfw_Shutdown();
    ImGui::DestroyContext();
    // glfw: terminate, clearing all previously allocated GLFW resources.
    glfwTerminate();
    return 0;
}

unsigned int quadVAO = 0;
unsigned int quadVBO;
void renderQuad()
{
    if (quadVAO == 0) {
	float quadVertices[] = {
	    // positions        // texture Coords
	    -1.0f, 1.0f, 0.0f, 0.0f, 1.0f, -1.0f, -1.0f, 0.0f, 0.0f, 0.0f,
	    1.0f,  1.0f, 0.0f, 1.0f, 1.0f, 1.0f,  -1.0f, 0.0f, 1.0f, 0.0f,
	};
	// setup plane VAO
	glGenVertexArrays(1, &quadVAO);
	glGenBuffers(1, &quadVBO);
	glBindVertexArray(quadVAO);
	glBindBuffer(GL_ARRAY_BUFFER, quadVBO);
	glBufferData(GL_ARRAY_BUFFER, sizeof(quadVertices), &quadVertices,
		     GL_STATIC_DRAW);
	glEnableVertexAttribArray(0);
	glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 5 * sizeof(float),
			      (void *)nullptr);
	glEnableVertexAttribArray(1);
	glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, 5 * sizeof(float),
			      (void *)(3 * sizeof(float)));
    }
    glBindVertexArray(quadVAO);
    glDrawArrays(GL_TRIANGLE_STRIP, 0, 4);
    glBindVertexArray(0);
}

// process all input: query GLFW whether relevant keys are pressed/released this
// frame and react accordingly
void processInput(GLFWwindow *window)
{
    if (glfwGetKey(window, GLFW_KEY_ESCAPE) == GLFW_PRESS)
	glfwSetWindowShouldClose(window, true);

    if (glfwGetKey(window, GLFW_KEY_W) == GLFW_PRESS)
	programState->camera.ProcessKeyboard(FORWARD, 4 * deltaTime);
    if (glfwGetKey(window, GLFW_KEY_S) == GLFW_PRESS)
	programState->camera.ProcessKeyboard(BACKWARD, 4 * deltaTime);
    if (glfwGetKey(window, GLFW_KEY_A) == GLFW_PRESS)
	programState->camera.ProcessKeyboard(LEFT, 4 * deltaTime);
    if (glfwGetKey(window, GLFW_KEY_D) == GLFW_PRESS)
	programState->camera.ProcessKeyboard(RIGHT, 4 * deltaTime);

    if (glfwGetKey(window, GLFW_KEY_SPACE) == GLFW_PRESS && !hdrKeyPressed) {
	hdr = !hdr;
	hdrKeyPressed = true;
    }
    if (glfwGetKey(window, GLFW_KEY_SPACE) == GLFW_RELEASE) {
	hdrKeyPressed = false;
    }

    if (glfwGetKey(window, GLFW_KEY_B) == GLFW_PRESS && !bloomKeyPressed) {
	bloom = !bloom;
	bloomKeyPressed = true;
    }
    if (glfwGetKey(window, GLFW_KEY_B) == GLFW_RELEASE) {
	bloomKeyPressed = false;
    }

    if (glfwGetKey(window, GLFW_KEY_Q) == GLFW_PRESS) {
	if (exposure > 0.0f)
	    exposure -= 0.005f;
	else
	    exposure = 0.0f;
    } else if (glfwGetKey(window, GLFW_KEY_E) == GLFW_PRESS) {
	exposure += 0.005f;
    }
}

// glfw: whenever the window size changed (by OS or user resize) this callback
// function executes
void framebuffer_size_callback(GLFWwindow *window, int width, int height)
{
    // make sure the viewport matches the new window dimensions; note that width
    // and height will be significantly larger than specified on retina
    // displays.
    glViewport(0, 0, width, height);
}

// glfw: whenever the mouse moves, this callback is called
void mouse_callback(GLFWwindow *window, double xpos, double ypos)
{
    if (firstMouse) {
	lastX = xpos;
	lastY = ypos;
	firstMouse = false;
    }

    float xoffset = xpos - lastX;
    float yoffset =
	lastY - ypos; // reversed since y-coordinates go from bottom to top

    lastX = xpos;
    lastY = ypos;

    if (programState->CameraMouseMovementUpdateEnabled)
	programState->camera.ProcessMouseMovement(xoffset, yoffset);
}

// glfw: whenever the mouse scroll wheel scrolls, this callback is called
void scroll_callback(GLFWwindow *window, double xoffset, double yoffset)
{
    programState->camera.ProcessMouseScroll(yoffset);
}

void DrawImGui(ProgramState *programState)
{
    ImGui_ImplOpenGL3_NewFrame();
    ImGui_ImplGlfw_NewFrame();
    ImGui::NewFrame();

    {
	static float f = 0.0f;
	ImGui::Begin("Hello window");
	ImGui::Text("Hello text");
	ImGui::SliderFloat("Float slider", &f, 0.0, 1.0);
	ImGui::ColorEdit3("Background color",
			  (float *)&programState->clearColor);
	ImGui::DragFloat3("Backpack position",
			  (float *)&programState->island1Position);
	ImGui::DragFloat("Backpack scale", &programState->island1Scale, 0.05,
			 0.1, 4.0);

	ImGui::DragFloat("pointLight.constant",
			 &programState->pointLight.constant, 0.05, 0.0, 1.0);
	ImGui::DragFloat("pointLight.linear", &programState->pointLight.linear,
			 0.05, 0.0, 1.0);
	ImGui::DragFloat("pointLight.quadratic",
			 &programState->pointLight.quadratic, 0.05, 0.0, 1.0);
	ImGui::End();
    }

    {
	ImGui::Begin("Camera info");
	const Camera &c = programState->camera;
	ImGui::Text("Camera position: (%f, %f, %f)", c.Position.x, c.Position.y,
		    c.Position.z);
	ImGui::Text("(Yaw, Pitch): (%f, %f)", c.Yaw, c.Pitch);
	ImGui::Text("Camera front: (%f, %f, %f)", c.Front.x, c.Front.y,
		    c.Front.z);
	ImGui::Checkbox("Camera mouse update",
			&programState->CameraMouseMovementUpdateEnabled);
	ImGui::End();
    }

    ImGui::Render();
    ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());
}

void key_callback(GLFWwindow *window, int key, int scancode, int action,
		  int mods)
{
    if (key == GLFW_KEY_F1 && action == GLFW_PRESS) {
	programState->ImGuiEnabled = !programState->ImGuiEnabled;
	if (programState->ImGuiEnabled) {
	    programState->CameraMouseMovementUpdateEnabled = false;
	    glfwSetInputMode(window, GLFW_CURSOR, GLFW_CURSOR_NORMAL);
	} else {
	    glfwSetInputMode(window, GLFW_CURSOR, GLFW_CURSOR_DISABLED);
	}
    }
}

unsigned int loadCubemap(vector<std::string> faces)
{
    unsigned int textureID;
    glGenTextures(1, &textureID);
    glBindTexture(GL_TEXTURE_CUBE_MAP, textureID);

    int width, height, nrChannels;
    for (unsigned int i = 0; i < faces.size(); i++) {
	unsigned char *data =
	    stbi_load(faces[i].c_str(), &width, &height, &nrChannels, 0);
	if (data) {
	    glTexImage2D(GL_TEXTURE_CUBE_MAP_POSITIVE_X + i, 0, GL_RGB, width,
			 height, 0, GL_RGB, GL_UNSIGNED_BYTE, data);
	    stbi_image_free(data);
	} else {
	    std::cout << "Cubemap texture failed to load at path: " << faces[i]
		      << std::endl;
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