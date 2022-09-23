#pragma once

using namespace glm;

class RenderPass
{
public:
	GLuint FBO = 0;
	GLuint VAO;
	GLuint VBO;
	std::vector<GLuint> m_colorAttachments;
	GLuint m_shaderProgram = -1;
	int m_width = 512;
	int m_height = 512;

	RenderPass(int width, int height) :m_width(width), m_height(height) { }

	void bindData(bool finalPass = false) {
		if (!finalPass)
			glGenFramebuffers(1, &FBO);
		glBindFramebuffer(GL_FRAMEBUFFER, FBO);

		glGenBuffers(1, &VBO);
		glBindBuffer(GL_ARRAY_BUFFER, VBO);
		std::vector<vec3> square = { vec3(-1, -1, 0), vec3(1, -1, 0), vec3(-1, 1, 0), vec3(1, 1, 0), vec3(-1, 1, 0), vec3(1, -1, 0) };
		glBufferData(GL_ARRAY_BUFFER, sizeof(vec3) * square.size(), NULL, GL_STATIC_DRAW);
		glBufferSubData(GL_ARRAY_BUFFER, 0, sizeof(vec3) * square.size(), &square[0]);

		glGenVertexArrays(1, &VAO);
		glBindVertexArray(VAO);
		glEnableVertexAttribArray(0);   // layout (location = 0) 
		glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 0, (GLvoid*)0);
		// 不是 finalPass 则生成帧缓冲的颜色附件
		if (!finalPass) {
			std::vector<GLuint> attachments;
			for (int i = 0; i < m_colorAttachments.size(); i++) {
				glBindTexture(GL_TEXTURE_2D, m_colorAttachments[i]);
				glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0 + i, GL_TEXTURE_2D, m_colorAttachments[i], 0);// 将颜色纹理绑定到 i 号颜色附件
				attachments.push_back(GL_COLOR_ATTACHMENT0 + i);
			}
			glDrawBuffers(attachments.size(), &attachments[0]);
		}

		glBindFramebuffer(GL_FRAMEBUFFER, 0);
	}

	void draw(std::vector<GLuint> texPassArray = {}) {
		glUseProgram(m_shaderProgram);
		glBindFramebuffer(GL_FRAMEBUFFER, FBO);
		glBindVertexArray(VAO);
		// 传上一帧的帧缓冲颜色附件
		for (int i = 0; i < texPassArray.size(); i++) {
			glActiveTexture(GL_TEXTURE0 + i);
			glBindTexture(GL_TEXTURE_2D, texPassArray[i]);
			//std::string uName = "texPass" + std::to_string(i);
			//glUniform1i(glGetUniformLocation(program, uName.c_str()), i);
		}
		glViewport(0, 0, m_width, m_height);
		glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
		glDrawArrays(GL_TRIANGLES, 0, 6);

		glBindVertexArray(0);
		glBindFramebuffer(GL_FRAMEBUFFER, 0);
		glUseProgram(0);
	}

	void setSize(int& width, int& height)
	{
		m_width = width;
		m_height = height;
	}
};
