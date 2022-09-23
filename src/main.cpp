#include <iostream>
#include <string>
#include <sstream>
#include <vector>
#include <iosfwd>
#include <algorithm>
#include <iomanip>

#define GLEW_STATIC
#include <GL/glew.h>
#include <GLFW/glfw3.h>
#define STB_IMAGE_IMPLEMENTATION
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/type_ptr.hpp>

#include "SBootManager.h"
#include "SShader.h"
#include "SFileSystem.h"
#include "SCamera.h"
#include "STexture.h"
#include "utils.h"
#include "hdrloader.h"
#include "RenderPass.h"

const GLuint WIDTH = 1280;
const GLuint HEIGHT = 1280;

glm::vec3 cameraPos = glm::vec3(0.0f, 0.0f, 3.0f);
glm::vec3 cameraFront = glm::vec3(0.0f, 0.0f, -1.0f);
glm::vec3 cameraUp = glm::vec3(0.0f, 1.0f, 0.0f);

float deltaTime = 0.0f;
float lastFrame = 0.0f;

void framebuffer_size_callback(GLFWwindow* window, int m_width, int m_height);
void mouse_callback(GLFWwindow* window, double xpos, double ypos);
void scroll_callback(GLFWwindow* window, double xoffset, double yoffset);
void processInput(GLFWwindow* window);

SCamera mainCamera(glm::vec3(0.0f, 0.0f, 10.0f));
float lastX = WIDTH / 2.0f;
float lastY = HEIGHT / 2.0f;
bool firstMouse = true;

// 绘制
clock_t t1, t2;
double dt, fps;
unsigned int frameCounter = 0;

// 相机参数
float upAngle = 0.0;
float rotatAngle = 0.0;
float r = 4.0;

int main()
{
	SBootManager& bootManager = SBootManager::getInstance();
	bootManager.initGlfw();
	GLFWwindow* window = glfwCreateWindow(WIDTH, HEIGHT, "LearnOpenGL", nullptr, nullptr);
	bootManager.initWindow(window);

	glfwSetFramebufferSizeCallback(window, framebuffer_size_callback);
	glfwSetCursorPosCallback(window, mouse_callback);
	glfwSetScrollCallback(window, scroll_callback);

	// scene config
	std::vector<Triangle> triangles;

	Material m;

	//金属球
	m.baseColor = vec3(0.8, 0.8, 0.8);
	m.roughness = 0.15;
	m.metallic = 1.0;
	m.clearcoat = 1.0;

	readObj(SFileSystem::getPath("res/models/sphere2.obj"), triangles, m, getTransformMatrix(vec3(0, 0, 0), vec3(0.5, 0, 0), vec3(0.75, 0.75, 0.75)), true);

	//非金属
	m.baseColor = vec3(1, 0.5, 0.5);
	m.roughness = 0.1;
	m.metallic = 0.0;
	m.clearcoat = 1.0;
	m.subsurface = 1.0;
	m.clearcoatGloss = 0.05;
	readObj(SFileSystem::getPath("res/models/sphere2.obj"), triangles, m, getTransformMatrix(vec3(0, 0, 0), vec3(-0.5, 0, 0), vec3(0.75, 0.75, 0.75)), true);

	//光源
	m.baseColor = WHITE;
	m.emissive = vec3(50, 50, 50);
	readObj(SFileSystem::getPath("res/models/quad.obj"), triangles, m, getTransformMatrix(vec3(0, 0, 0), vec3(0.0, 2.5, 0.0), vec3(1, 0.01, 1)), false);

	int nTriangles = triangles.size();
	std::cout << "模型读取完成: 共 " << nTriangles << " 个三角形" << std::endl;

	// ----------------------------------------------------------------------------- //

	// 建立 bvh
	BVHNode testNode;
	testNode.left = 255;
	testNode.right = 128;
	testNode.n = 30;
	testNode.AA = vec3(1, 1, 0);
	testNode.BB = vec3(0, 1, 0);
	std::vector<BVHNode> nodes{ testNode };
	//buildBVH(triangles, nodes, 0, triangles.size() - 1, 8);
	buildBVHwithSAH(triangles, nodes, 0, triangles.size() - 1, 8);
	int nNodes = nodes.size();
	std::cout << "BVH 建立完成: 共 " << nNodes << " 个节点" << std::endl;

	// 编码 三角形, 材质
	std::vector<Triangle_encoded> triangles_encoded(nTriangles);
	for (int i = 0; i < nTriangles; i++) {
		Triangle& t = triangles[i];
		Material& m = t.material;
		// 顶点位置
		triangles_encoded[i].p1 = t.p1;
		triangles_encoded[i].p2 = t.p2;
		triangles_encoded[i].p3 = t.p3;
		// 顶点法线
		triangles_encoded[i].n1 = t.n1;
		triangles_encoded[i].n2 = t.n2;
		triangles_encoded[i].n3 = t.n3;
		// 材质
		triangles_encoded[i].emissive = m.emissive;
		triangles_encoded[i].baseColor = m.baseColor;
		triangles_encoded[i].param1 = vec3(m.subsurface, m.metallic, m.specular);
		triangles_encoded[i].param2 = vec3(m.specularTint, m.roughness, m.anisotropic);
		triangles_encoded[i].param3 = vec3(m.sheen, m.sheenTint, m.clearcoat);
		triangles_encoded[i].param4 = vec3(m.clearcoatGloss, m.IOR, m.transmission);
	}

	// 编码 BVHNode, aabb
	std::vector<BVHNode_encoded> nodes_encoded(nNodes);
	for (int i = 0; i < nNodes; i++) {
		nodes_encoded[i].childs = vec3(nodes[i].left, nodes[i].right, 0);
		nodes_encoded[i].leafInfo = vec3(nodes[i].n, nodes[i].index, 0);
		nodes_encoded[i].AA = nodes[i].AA;
		nodes_encoded[i].BB = nodes[i].BB;
	}

	// ----------------------------------------------------------------------------- //

	GLuint trianglesTextureBuffer;
	GLuint bvhNodesTextureBuffer;
	GLuint lastFrame;
	GLuint hdrMap;

	RenderPass pass1(WIDTH, HEIGHT);
	RenderPass pass2(WIDTH, HEIGHT);
	RenderPass pass3(WIDTH, HEIGHT);

	// 三角形数组
	GLuint tbo0;
	glGenBuffers(1, &tbo0);
	glBindBuffer(GL_TEXTURE_BUFFER, tbo0);
	glBufferData(GL_TEXTURE_BUFFER, triangles_encoded.size() * sizeof(Triangle_encoded), &triangles_encoded[0], GL_STATIC_DRAW);
	glGenTextures(1, &trianglesTextureBuffer);
	glBindTexture(GL_TEXTURE_BUFFER, trianglesTextureBuffer);
	glTexBuffer(GL_TEXTURE_BUFFER, GL_RGB32F, tbo0);

	// BVHNode 数组
	GLuint nbo0;
	glGenBuffers(1, &nbo0);
	glBindBuffer(GL_TEXTURE_BUFFER, nbo0);
	glBufferData(GL_TEXTURE_BUFFER, nodes_encoded.size() * sizeof(BVHNode_encoded), &nodes_encoded[0], GL_STATIC_DRAW);
	glGenTextures(1, &bvhNodesTextureBuffer);
	glBindTexture(GL_TEXTURE_BUFFER, bvhNodesTextureBuffer);
	glTexBuffer(GL_TEXTURE_BUFFER, GL_RGB32F, nbo0);

	// hdr 全景图
	HDRLoaderResult hdrRes;
	bool r = HDRLoader::load(SFileSystem::getPath("res/texture/circus_arena_4k.hdr").c_str(), hdrRes);
	hdrMap = getTextureRGB32F(hdrRes.m_width, hdrRes.m_height);
	glTexImage2D(GL_TEXTURE_2D, 0, GL_RGB32F, hdrRes.m_width, hdrRes.m_height, 0, GL_RGB, GL_FLOAT, hdrRes.cols);

	SShader shader1(
		SFileSystem::getPath("res/shader/vshader.vert").c_str(),
		SFileSystem::getPath("res/shader/fshader.frag").c_str());
	SShader shader2(
		SFileSystem::getPath("res/shader/vshader.vert").c_str(),
		SFileSystem::getPath("res/shader/pass2.frag").c_str());
	SShader shader3(
		SFileSystem::getPath("res/shader/vshader.vert").c_str(),
		SFileSystem::getPath("res/shader/pass3.frag").c_str());

	pass1.m_shaderProgram = shader1.ID;
	pass1.m_colorAttachments.push_back(getTextureRGB32F(pass1.m_width, pass1.m_height));
	pass1.m_colorAttachments.push_back(getTextureRGB32F(pass1.m_width, pass1.m_height));
	pass1.m_colorAttachments.push_back(getTextureRGB32F(pass1.m_width, pass1.m_height));
	pass1.bindData();

	glUseProgram(pass1.m_shaderProgram);
	glUniform1i(glGetUniformLocation(pass1.m_shaderProgram, "nTriangles"), triangles.size());
	glUniform1i(glGetUniformLocation(pass1.m_shaderProgram, "nNodes"), nodes.size());
	glUniform1i(glGetUniformLocation(pass1.m_shaderProgram, "width"), pass1.m_width);
	glUniform1i(glGetUniformLocation(pass1.m_shaderProgram, "height"), pass1.m_height);
	glUseProgram(0);

	pass2.m_shaderProgram = shader2.ID;
	lastFrame = getTextureRGB32F(pass2.m_width, pass2.m_height);
	pass2.m_colorAttachments.push_back(lastFrame);
	pass2.bindData();

	pass3.m_shaderProgram = shader3.ID;
	pass3.bindData(true);

	// ----------------------------------------------------------------------------- //

	std::cout << "开始..." << std::endl << std::endl;

	glEnable(GL_DEPTH_TEST);            // 开启深度测试
	glClearColor(0.0, 0.0, 0.0, 1.0);   // 背景颜色 -- 黑

	while (!glfwWindowShouldClose(window))
	{
		// 帧计时
		t2 = clock();
		dt = (double)(t2 - t1) / CLOCKS_PER_SEC;
		fps = 1.0 / dt;
		std::cout << "\r";
		std::cout << std::fixed << std::setprecision(2) << "FPS : " << fps << "    迭代次数: " << frameCounter;
		t1 = t2;

		mat4 cameraRotate = mainCamera.GetViewMatrix();
		cameraRotate = inverse(cameraRotate);

		// 传 uniform 给 pass1
		glUseProgram(pass1.m_shaderProgram);
		glUniform3fv(glGetUniformLocation(pass1.m_shaderProgram, "eye"), 1, value_ptr(mainCamera.Position));
		glUniformMatrix4fv(glGetUniformLocation(pass1.m_shaderProgram, "cameraRotate"), 1, GL_FALSE, value_ptr(cameraRotate));
		glUniform1ui(glGetUniformLocation(pass1.m_shaderProgram, "frameCounter"), frameCounter++);// 传计数器用作随机种子

		glActiveTexture(GL_TEXTURE0);
		glBindTexture(GL_TEXTURE_BUFFER, trianglesTextureBuffer);
		glUniform1i(glGetUniformLocation(pass1.m_shaderProgram, "triangles"), 0);

		glActiveTexture(GL_TEXTURE1);
		glBindTexture(GL_TEXTURE_BUFFER, bvhNodesTextureBuffer);
		glUniform1i(glGetUniformLocation(pass1.m_shaderProgram, "nodes"), 1);

		glActiveTexture(GL_TEXTURE2);
		glBindTexture(GL_TEXTURE_2D, lastFrame);
		glUniform1i(glGetUniformLocation(pass1.m_shaderProgram, "lastFrame"), 2);

		glActiveTexture(GL_TEXTURE3);
		glBindTexture(GL_TEXTURE_2D, hdrMap);
		glUniform1i(glGetUniformLocation(pass1.m_shaderProgram, "hdrMap"), 3);

		// 绘制
		pass1.draw();
		pass2.draw(pass1.m_colorAttachments);
		pass3.draw(pass2.m_colorAttachments);

		glfwSwapBuffers(window);
		glfwPollEvents();
	}

	bootManager.terminate();

	return 0;
}

void processInput(GLFWwindow* window)
{
	if (glfwGetKey(window, GLFW_KEY_ESCAPE) == GLFW_PRESS)
		glfwSetWindowShouldClose(window, true);

	if (glfwGetKey(window, GLFW_KEY_W) == GLFW_PRESS)
		mainCamera.ProcessKeyboard(FORWARD, deltaTime);
	if (glfwGetKey(window, GLFW_KEY_S) == GLFW_PRESS)
		mainCamera.ProcessKeyboard(BACKWARD, deltaTime);
	if (glfwGetKey(window, GLFW_KEY_A) == GLFW_PRESS)
		mainCamera.ProcessKeyboard(LEFT, deltaTime);
	if (glfwGetKey(window, GLFW_KEY_D) == GLFW_PRESS)
		mainCamera.ProcessKeyboard(RIGHT, deltaTime);
}

void framebuffer_size_callback(GLFWwindow* window, int m_width, int m_height)
{
	glViewport(0, 0, m_width, m_height);
}

void mouse_callback(GLFWwindow* window, double xpos, double ypos)
{
	if (firstMouse)
	{
		lastX = xpos;
		lastY = ypos;
		firstMouse = false;
	}

	float xoffset = xpos - lastX;
	float yoffset = lastY - ypos;

	lastX = xpos;
	lastY = ypos;

	mainCamera.ProcessMouseMovement(xoffset, yoffset);
}

void scroll_callback(GLFWwindow* window, double xoffset, double yoffset)
{
	mainCamera.ProcessMouseScroll(yoffset);
}
