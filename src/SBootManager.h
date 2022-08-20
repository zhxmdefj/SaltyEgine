#pragma once

#include <iostream>

#define GLEW_STATIC
#include <GL/glew.h>
#include <GLFW/glfw3.h>


class SBootManager
{
public:
	static SBootManager& getInstance()
	{
		static SBootManager value;
		return value;
	}

	bool initGlfw() const;
	bool initWindow(GLFWwindow* window);
	void terminate();
	//void processInput(GLFWwindow* window);

private:

	SBootManager() = default;
	SBootManager(const SBootManager& other) = delete;
	SBootManager& operator=(const SBootManager&) = delete;

};