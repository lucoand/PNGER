#include "main.h"
#include "png.h"

float verticies[] = {
    // positions   // tex coords
    -1.0f, -1.0f, 0.0f, 0.0f, // bottom left
    1.0f,  -1.0f, 1.0f, 0.0f, // bottom right
    1.0f,  1.0f,  1.0f, 1.0f, // top right
    -1.0f, 1.0f,  0.0f, 1.0f, // top left
};

unsigned int indices[] = {
    0, 1, 2, // first triangle
    2, 3, 0  // second triangle
};

static const char *vertex_shader_text =
    "#version 330 core\n"
    "layout (location = 0) in vec2 aPos;\n"
    "layout (location = 1) in vec2 aTexCoord;\n"
    "out vec2 TexCoord;\n"
    "void main()\n"
    "{\n"
    "    gl_Position = vec4(aPos.xy, 0.0, 1.0);\n"
    "    TexCoord = aTexCoord;\n"
    "}\n";

static const char *fragment_shader_text =
    "#version 330 core\n"
    "in vec2 TexCoord;\n"
    "out vec4 FragColor;\n"
    "uniform sampler2D tex;\n"
    "void main()\n"
    "{\n"
    "    FragColor = texture(tex, vec2(TexCoord.x, 1.0 - TexCoord.y));\n"
    // "    FragColor = vec4(1.0, 1.0, 0.0, 1.0);\n"
    // "FragColor = vec4(TexCoord, 0.0, 1.0);\n"
    "}\n";

static void error_callback(int error, const char *description) {
  fprintf(stderr, "Error: %s\n", description);
}

static void key_callback(GLFWwindow *window, int key, int scancode, int action,
                         int mods) {
  if (key == GLFW_KEY_ESCAPE && action == GLFW_PRESS)
    glfwSetWindowShouldClose(window, GLFW_TRUE);
}

int main(int argc, char **argv) {
  if (argc < 2) {
    printf("Requires one argument.  Should be a png file.\n");
    return 1;
  }
  if (argc > 2) {
    printf("Should only be one argument.\n");
    return 1;
  }
  FILE *f = fopen(argv[1], "rb");
  if (!f) {
    perror("fopen");
    return 1;
  }
  PNG *png = decode_PNG(f);
  fclose(f);
  if (!png) {
    fprintf(stderr, "Unable to decode png.\n");
    return 1;
  }

  int width = (int)png->header->width;
  int height = (int)png->header->height;

  glfwSetErrorCallback(error_callback);

  if (!glfwInit()) {
    fprintf(stderr, "Unable to initialize openGL.\n");
    free_PNG(png);
    exit(EXIT_FAILURE);
  }

  glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
  glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 3);
  glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);

  GLFWwindow *window =
      glfwCreateWindow(width, height, "PNGER v0.03", NULL, NULL);
  if (!window) {
    fprintf(stderr, "Unable to create window.\n");
    glfwTerminate();
    free_PNG(png);
    exit(EXIT_FAILURE);
  }

  glfwSetKeyCallback(window, key_callback);

  glfwMakeContextCurrent(window);
  gladLoadGL(glfwGetProcAddress);
  glfwSwapInterval(1);

  GLuint vertex_buffer;
  glGenBuffers(1, &vertex_buffer);
  glBindBuffer(GL_ARRAY_BUFFER, vertex_buffer);
  glBufferData(GL_ARRAY_BUFFER, sizeof(verticies), verticies, GL_STATIC_DRAW);

  const GLuint vertex_shader = glCreateShader(GL_VERTEX_SHADER);
  glShaderSource(vertex_shader, 1, &vertex_shader_text, NULL);
  glCompileShader(vertex_shader);
  GLint compiled;
  glGetShaderiv(vertex_shader, GL_COMPILE_STATUS, &compiled);
  if (!compiled) {
    char buf[512];
    glGetShaderInfoLog(vertex_shader, sizeof(buf), NULL, buf);
    fprintf(stderr, "Vertex Shader Error:\n%s\n", buf);
    glfwDestroyWindow(window);
    glfwTerminate();
    free_PNG(png);
    exit(EXIT_FAILURE);
  }

  const GLuint fragment_shader = glCreateShader(GL_FRAGMENT_SHADER);
  glShaderSource(fragment_shader, 1, &fragment_shader_text, NULL);
  glCompileShader(fragment_shader);
  glGetShaderiv(vertex_shader, GL_COMPILE_STATUS, &compiled);
  if (!compiled) {
    char buf[512];
    glGetShaderInfoLog(fragment_shader, sizeof(buf), NULL, buf);
    fprintf(stderr, "Fragment Shader Error:\n%s\n", buf);
    glfwDestroyWindow(window);
    glfwTerminate();
    free_PNG(png);
    exit(EXIT_FAILURE);
  }

  const GLuint program = glCreateProgram();
  glAttachShader(program, vertex_shader);
  glAttachShader(program, fragment_shader);
  glLinkProgram(program);

  GLint linked;
  glGetProgramiv(program, GL_LINK_STATUS, &linked);
  if (!linked) {
    char buf[512];
    glGetProgramInfoLog(program, sizeof(buf), NULL, buf);
    fprintf(stderr, "Program Link Error:\n%s\n", buf);
    glfwDestroyWindow(window);
    glfwTerminate();
    free_PNG(png);
    exit(EXIT_FAILURE);
  }

  // glEnable(GL_TEXTURE_2D);

  glDeleteShader(vertex_shader);
  glDeleteShader(fragment_shader);

  glUseProgram(program);
  GLint texLoc = glGetUniformLocation(program, "tex");
  if (texLoc != -1) {
    glUniform1i(texLoc, 0);
  } else {
    fprintf(stderr, "Could not find 'tex' uniform!\n");
  }

  GLuint vao;
  glGenVertexArrays(1, &vao);
  glBindVertexArray(vao);

  glBindBuffer(GL_ARRAY_BUFFER, vertex_buffer);

  glEnableVertexAttribArray(0); // position
  glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, 4 * sizeof(float), (void *)0);

  glEnableVertexAttribArray(1); // texcoord
  glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, 4 * sizeof(float),
                        (void *)(2 * sizeof(float)));

  GLuint ebo;
  glGenBuffers(1, &ebo);
  glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, ebo);
  glBufferData(GL_ELEMENT_ARRAY_BUFFER, sizeof(indices), indices,
               GL_STATIC_DRAW);

  GLuint tex;
  glGenTextures(1, &tex);
  glBindTexture(GL_TEXTURE_2D, tex);

  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);

  switch (png->header->pixel_format) {
  case RGBA:
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, width, height, 0, GL_RGBA,
                 GL_UNSIGNED_BYTE, png->pixels);
    break;
  case RGB:
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGB, width, height, 0, GL_RGB,
                 GL_UNSIGNED_BYTE, png->pixels);
    break;
  case GSA:
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RG, width, height, 0, GL_RG,
                 GL_UNSIGNED_BYTE, png->pixels);
    break;
  default:
    fprintf(stderr,
            "Texture generation not yet implemented for color mode %d.\n",
            png->header->color_type);
    glfwDestroyWindow(window);
    glfwTerminate();
    free_PNG(png);
    exit(EXIT_FAILURE);
    break;
  }

  switch (png->header->pixel_format) {
  case GSA:
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_SWIZZLE_A, GL_GREEN);
  case GS:
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_SWIZZLE_R, GL_RED);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_SWIZZLE_G, GL_RED);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_SWIZZLE_B, GL_RED);
    break;
  default:
    break;
  }

  GLenum err = glGetError();
  if (err != GL_NO_ERROR) {
    printf("GL Error after glTexImage2D: 0x%x\n", err);
  }

  if (png->header->pixel_format == RGBA || png->header->pixel_format == GSA) {
    // printf("We blending\n");
    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
  }

  while (!glfwWindowShouldClose(window)) {
    glfwGetFramebufferSize(window, &width, &height);

    glViewport(0, 0, width, height);
    // glClearColor(1.0f, 0.0f, 1.0f, 1.0f);
    glClear(GL_COLOR_BUFFER_BIT);

    glBindVertexArray(vao);
    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, tex);
    glUseProgram(program);
    glDrawElements(GL_TRIANGLES, 6, GL_UNSIGNED_INT, 0);

    glfwSwapBuffers(window);
    glfwPollEvents();
  }

  glfwDestroyWindow(window);

  glfwTerminate();
  free_PNG(png);
  exit(EXIT_SUCCESS);
}
