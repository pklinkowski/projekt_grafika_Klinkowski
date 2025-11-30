#include "pch.h"
#include <iostream>
#include <SFML/Window.hpp>
#include <SFML/Graphics.hpp>
#include <GL/gl.h>
#include <GL/glu.h>
#include <array>
#include <stdlib.h>

typedef sf::Event sfe;
typedef sf::Keyboard sfk;

namespace {
    struct AppState {
		// camera configuration
        sf::Vector3f eye{ 0.0f, 20.0f, 15.0f };
        sf::Vector3f center{ 0.0f, 0.0f, 0.0f };
        sf::Vector3f up{ 0.0f, 1.0f, 0.0f };

		bool top_down_view = false;

        // projection
        float fovDeg = 60.0f;
        float nearP = 0.1f; 
        float farP = 100.0f;

        // player positions
        const float player1_x = 12.0f;
        const float player2_x = -12.0f;
        float player1_z = 0.0f;
        float player2_z = 0.0f;

        // paddle properties
        const float paddle_speed = 15.0f;
        const float paddle_length = 4.0f;
        const float paddle_width = 2.0f;
        const float paddle_height = 2.0f;

        // ball properties
        sf::Vector3f ball_pos{ 0.0f, 0.6f, 0.0f };
        sf::Vector3f ball_vel{ 16.0f, 0.0f, 7.0f };

        // arena bounds
        const float arena_half_width = 12.0f;
        const float arena_half_length = 7.0f;
    } G;

	// utility functions
    float clamp(float v, float a, float b) { return (v < a ? a : (v > b ? b : v)); }
}

namespace {
    struct GameLogic {
		int score1 = 0;
		int score2 = 0;

		bool round_active = true;
		float round_timer = 0.0f;
		const float round_restart_delay = 2.0f;

        float ball_speed = 24.0f;
    } GL;
}

namespace {
    struct AppTextures {
        GLuint wallTexID = 0;
		GLuint floorTexID = 1;
    } T;
}

// OpenGL initialization and setup functions
static void initOpenGL() {
    glClearColor(0.12f, 0.13f, 0.16f, 1.0f);
    glEnable(GL_DEPTH_TEST);
    glDepthFunc(GL_LEQUAL);

	GLfloat light_pos[] = { 0.0f, 20.0f, 0.0f, 1.0f };
	glLightfv(GL_LIGHT0, GL_POSITION, light_pos);

    GLfloat lightAmbient[] = { 0.2f, 0.2f, 0.2f, 1.0f };
    GLfloat lightDiffuse[] = { 0.9f, 0.9f, 0.9f, 1.0f };
    GLfloat lightSpecular[] = { 1.0f, 1.0f, 1.0f, 1.0f };

    glLightfv(GL_LIGHT0, GL_AMBIENT, lightAmbient);
    glLightfv(GL_LIGHT0, GL_DIFFUSE, lightDiffuse);
    glLightfv(GL_LIGHT0, GL_SPECULAR, lightSpecular);
}

// activate lighting
static void setupLight() {
	glEnable(GL_LIGHTING);
	glEnable(GL_LIGHT0);
	glShadeModel(GL_SMOOTH);
	glEnable(GL_NORMALIZE);
}

// sets the projection matrix based on the window size
static void setupProjection(sf::Vector2u s) {
    if (!s.y) s.y = 1;
    const double aspect = s.x / static_cast<double>(s.y);
    glViewport(0, 0, (GLsizei)s.x, (GLsizei)s.y);
    glMatrixMode(GL_PROJECTION);
    glLoadIdentity();
    gluPerspective(G.fovDeg, aspect, G.nearP, G.farP);
    glMatrixMode(GL_MODELVIEW);
}

// sets up the camera
static void setupView() {
    glMatrixMode(GL_MODELVIEW);
    glLoadIdentity();

    if (G.top_down_view) {
        gluLookAt(0.0f, 30.0f, 0.0f,
            0.0f, 0.0f, 0.0f,
            0.0f, 0.0f, -1.0f);
        return;
	}
    else {
        gluLookAt(G.eye.x, G.eye.y, G.eye.z,
            G.center.x, G.center.y, G.center.z,
            G.up.x, G.up.y, G.up.z);
    }
}

static GLUquadric* gQuad = nullptr;

// initializes the quadric object for drawing spheres
static void initQuadric() {
    gQuad = gluNewQuadric();
}

// frees the quadric object
static void freeQuadric() {
    if (gQuad) { gluDeleteQuadric(gQuad); gQuad = nullptr; }
}

// draws a sphere (ball) with given radius
static void drawBall(float radius, int slices = 32, int stacks = 16) {
    // material definition for ball
    GLfloat mat_ambient[] = { 1.0f, 1.0f, 0.3f, 1.0f };
    GLfloat mat_diffuse[] = { 1.0f, 1.0f, 0.3f, 1.0f };
    GLfloat mat_specular[] = { 1.0f, 1.0f, 1.0f, 1.0f };
    GLfloat shininess[] = { 80.0f };

    glMaterialfv(GL_FRONT_AND_BACK, GL_AMBIENT, mat_ambient);
    glMaterialfv(GL_FRONT_AND_BACK, GL_DIFFUSE, mat_diffuse);
    glMaterialfv(GL_FRONT_AND_BACK, GL_SPECULAR, mat_specular);
    glMaterialfv(GL_FRONT_AND_BACK, GL_SHININESS, shininess);

    gluSphere(gQuad, radius, slices, stacks);
}

// draws a paddle at given position with specified dimensions and color
static void drawPaddle(float p_x, float p_z, float paddle_length, float paddle_width, float paddle_height, float color_r, float color_g, float color_b) {
    glMatrixMode(GL_MODELVIEW);
    glPushMatrix();             
    glTranslatef(p_x, 0.0f, p_z); 

    // paddle material
    GLfloat mat_diffuse[] = { color_r, color_g, color_b, 1.0f };
    GLfloat mat_ambient[] = { color_r * 0.3f, color_g * 0.3f, color_b * 0.3f, 1.0f };
    GLfloat mat_specular[] = { 0.2f, 0.2f, 0.2f, 1.0f };
    GLfloat shininess[] = { 5.0f };

    glMaterialfv(GL_FRONT_AND_BACK, GL_AMBIENT, mat_ambient);
    glMaterialfv(GL_FRONT_AND_BACK, GL_DIFFUSE, mat_diffuse);
    glMaterialfv(GL_FRONT_AND_BACK, GL_SPECULAR, mat_specular);
    glMaterialfv(GL_FRONT_AND_BACK, GL_SHININESS, shininess);


    glBegin(GL_QUADS);             
    
    // top
    glNormal3f(0, 1, 0);
	glVertex3f(-paddle_width / 2, paddle_height, paddle_length / 2);
    glVertex3f(paddle_width / 2, paddle_height, paddle_length / 2);
    glVertex3f(paddle_width / 2, paddle_height, -paddle_length / 2);
    glVertex3f(-paddle_width / 2, paddle_height, -paddle_length / 2);
    
    // bottom
	glNormal3f(0, -1, 0);
    glVertex3f(-paddle_width / 2, 0.0f, -paddle_length / 2);
    glVertex3f(paddle_width / 2, 0.0f, -paddle_length / 2);
    glVertex3f(paddle_width / 2, 0.0f, paddle_length / 2);
    glVertex3f(-paddle_width / 2, 0.0f, paddle_length / 2);
    
    // front
	glNormal3f(0, 0, 1);
    glVertex3f(-paddle_width / 2, 0.0f, paddle_length / 2);
    glVertex3f(paddle_width / 2, 0.0f, paddle_length / 2);
    glVertex3f(paddle_width / 2, paddle_height, paddle_length / 2);
    glVertex3f(-paddle_width / 2, paddle_height, paddle_length / 2);
    
    // back
	glNormal3f(0, 0, -1);
    glVertex3f(-paddle_width / 2, paddle_height, -paddle_length / 2);
    glVertex3f(paddle_width / 2, paddle_height, -paddle_length / 2);
    glVertex3f(paddle_width / 2, 0.0f, -paddle_length / 2);
    glVertex3f(-paddle_width / 2, 0.0f, -paddle_length / 2);
    
    // right
	glNormal3f(1, 0, 0);
    glVertex3f(paddle_width / 2, 0.0f, -paddle_length / 2);
    glVertex3f(paddle_width / 2, paddle_height, -paddle_length / 2);
    glVertex3f(paddle_width / 2, paddle_height, paddle_length / 2);
    glVertex3f(paddle_width / 2, 0.0f, paddle_length / 2);
    
    // left
	glNormal3f(-1, 0, 0);
	glVertex3f(-paddle_width / 2, 0.0f, paddle_length / 2);
    glVertex3f(-paddle_width / 2, paddle_height, paddle_length / 2);
    glVertex3f(-paddle_width / 2, paddle_height, -paddle_length / 2);
    glVertex3f(-paddle_width / 2, 0.0f, -paddle_length / 2);
    
    glEnd();
    glPopMatrix();
}

// draws a textured cuboid with given coordinates and color
static void drawCuboidTex(float x1, float x2,
    float y1, float y2,
    float z1, float z2,
    float color_r, float color_g, float color_b)
{
    glPushMatrix();
       
    // material setup
    GLfloat mat_ambient[] = { color_r * 0.3f, color_g * 0.3f, color_b * 0.3f, 1.0f };
    GLfloat mat_diffuse[] = { color_r,        color_g,        color_b,        1.0f };
    GLfloat mat_specular[] = { 0.2f, 0.2f, 0.2f, 1.0f };
    GLfloat shininess[] = { 8.0f };

    glMaterialfv(GL_FRONT_AND_BACK, GL_AMBIENT, mat_ambient);
    glMaterialfv(GL_FRONT_AND_BACK, GL_DIFFUSE, mat_diffuse);
    glMaterialfv(GL_FRONT_AND_BACK, GL_SPECULAR, mat_specular);
    glMaterialfv(GL_FRONT_AND_BACK, GL_SHININESS, shininess);

	// UV tiling factors
    float U = 1.0f;
    float V = 1.0f;

    glBegin(GL_QUADS);

    glNormal3f(0, -1, 0);
    glTexCoord2f(0, 0); glVertex3f(x1, y2, z1);
    glTexCoord2f(U, 0); glVertex3f(x2, y2, z1);
    glTexCoord2f(U, V); glVertex3f(x2, y2, z2);
    glTexCoord2f(0, V); glVertex3f(x1, y2, z2);

    glNormal3f(0, 1, 0);
    glTexCoord2f(0, 0); glVertex3f(x1, y1, z2);
    glTexCoord2f(U, 0); glVertex3f(x2, y1, z2);
    glTexCoord2f(U, V); glVertex3f(x2, y1, z1);
    glTexCoord2f(0, V); glVertex3f(x1, y1, z1);

    glNormal3f(0, 0, -1);
    glTexCoord2f(0, 0); glVertex3f(x1, y1, z2);
    glTexCoord2f(U, 0); glVertex3f(x2, y1, z2);
    glTexCoord2f(U, V); glVertex3f(x2, y2, z2);
    glTexCoord2f(0, V); glVertex3f(x1, y2, z2);

    glNormal3f(0, 0, 1);
    glTexCoord2f(0, 0); glVertex3f(x2, y1, z1);
    glTexCoord2f(U, 0); glVertex3f(x1, y1, z1);
    glTexCoord2f(U, V); glVertex3f(x1, y2, z1);
    glTexCoord2f(0, V); glVertex3f(x2, y2, z1);

    glNormal3f(1, 0, 0);
    glTexCoord2f(0, 0); glVertex3f(x2, y1, z1);
    glTexCoord2f(U, 0); glVertex3f(x2, y1, z2);
    glTexCoord2f(U, V); glVertex3f(x2, y2, z2);
    glTexCoord2f(0, V); glVertex3f(x2, y2, z1);

    glNormal3f(-1, 0, 0);
    glTexCoord2f(0, 0); glVertex3f(x1, y1, z2);
    glTexCoord2f(U, 0); glVertex3f(x1, y1, z1);
    glTexCoord2f(U, V); glVertex3f(x1, y2, z1);
    glTexCoord2f(0, V); glVertex3f(x1, y2, z2);

    glEnd();
    glPopMatrix();
}


// draws a flat line on the ground between (x1, z1) and (x2, z2)
static void drawLine(float x1, float x2, float z1, float z2)
{
    glDisable(GL_LIGHTING);
    glColor3f(1.0f, 1.0f, 1.0f); 

    float y = 0.02f; 

    glBegin(GL_QUADS);
    glVertex3f(x1, y, z1);
    glVertex3f(x2, y, z1);
    glVertex3f(x2, y, z2);
    glVertex3f(x1, y, z2);
    glEnd();

    glEnable(GL_LIGHTING); 
}

// moves the paddle along the z-axis
static void moveCube(float dz, int cuboid) {
    if (cuboid == 1) {
        G.player1_z += dz;
    }
    else {
        G.player2_z += dz;
    }
}

// updates the ball position and handles wall collisions
static void updateBall(float dt) {
    G.ball_pos.x += G.ball_vel.x * dt;
    G.ball_pos.z += G.ball_vel.z * dt;

	// Z-axis wall collisions
    if (G.ball_pos.z > G.arena_half_length - 0.6f) {
        G.ball_pos.z = G.arena_half_length - 0.6f;
        G.ball_vel.z *= -1;
    }
    if (G.ball_pos.z < -G.arena_half_length + 0.6f) {
        G.ball_pos.z = -G.arena_half_length + 0.6f;
        G.ball_vel.z *= -1;
    }
}

// handles ball and paddle collisions
static void checkPaddleCollision() {
    float r = 0.6f;

    float half_w = G.paddle_width / 2.0f;
    float half_h = G.paddle_length / 2.0f;
    
	// determines the ball spin effect upon collision
    const float SPIN_FACTOR = 15.0f;

    auto collide = [&](float px, float pz, bool player1)
        {
            float dx = G.ball_pos.x - px;
            float dz = G.ball_pos.z - pz;

            if (fabs(dx) < half_w + r &&
                fabs(dz) < half_h + r)
            {
				// reverse x direction
                G.ball_vel.x *= -1;

                // spin effect based on collision point
                float offset = (G.ball_pos.z - pz) / half_h;
                G.ball_vel.z += offset * SPIN_FACTOR;

				// normalize velocity and gradually increase speed
                GL.ball_speed += 0.5f;
                float len = sqrt(G.ball_vel.x * G.ball_vel.x + G.ball_vel.z * G.ball_vel.z);
                G.ball_vel.x = (G.ball_vel.x / len) * GL.ball_speed;
                G.ball_vel.z = (G.ball_vel.z / len) * GL.ball_speed;

				// adjust ball position to avoid sticking
                G.ball_pos.x = px + (player1 ? -(half_w + r) : +(half_w + r));
            }
        };
	collide(G.player1_x, G.player1_z, true);
	collide(G.player2_x, G.player2_z, false);
}

// resets the ball position and velocity after scoring with a delay
static void resetBall(int direction)
{
    GL.ball_speed = 24.0f;

    G.ball_pos = { 0.0f, 0.6f, 0.0f };

    float baseSpeed = GL.ball_speed;

    // random slight angle deviation
    float angle = ((rand() % 41) - 20) * 3.14159f / 180.0f;

    G.ball_vel.x = cos(angle) * baseSpeed * direction;
    G.ball_vel.z = sin(angle) * baseSpeed * 0.5f;

    GL.round_active = false;
    GL.round_timer = GL.round_restart_delay;

    std::cout << "Score: " << GL.score1 << " : " << GL.score2 << "\n";
}

// checks if a player has scored
static void checkScore()
{
    if (G.ball_pos.x > G.arena_half_width-0.7) {
        GL.score1++;
        resetBall(-1);
    }
    else if (G.ball_pos.x < -G.arena_half_width+0.7) {
        GL.score2++;
        resetBall(1);
    }
}

// updates the round state and timer
static void updateRoundState(float dt)
{
    if (!GL.round_active) {
        GL.round_timer -= dt;
        if (GL.round_timer <= 0.0f) {
            GL.round_active = true;
        }
    }
}

// detects victory condition
static bool checkGameOver()
{
    const int WINNING_SCORE = 5;
    if (GL.score1 >= WINNING_SCORE) {
        std::cout << "Player 1 wins!\n";
        return true;
    }
    else if (GL.score2 >= WINNING_SCORE) {
        std::cout << "Player 2 wins!\n";
        return true;
    }
    return false;
}

// main per-frame game update function
static void gameUpdate(float dt)
{
    if (GL.round_active) {
        updateBall(dt);
        checkPaddleCollision();
        checkScore();
    }
    else {
        updateRoundState(dt);
    }
}

// handles paddle movement input
static void handleInput(float dt)
{
    float move = G.paddle_speed * dt;

	// player 1 (arrow keys)
    if (sf::Keyboard::isKeyPressed(sf::Keyboard::Left))
        G.player1_z += move;

    if (sf::Keyboard::isKeyPressed(sf::Keyboard::Right))
        G.player1_z -= move;

	// player 2 (A/D)
    if (sf::Keyboard::isKeyPressed(sf::Keyboard::A))
        G.player2_z += move;

    if (sf::Keyboard::isKeyPressed(sf::Keyboard::D))
        G.player2_z -= move;

    float half_len = G.paddle_length / 2.0f;

	// clamp paddle positions within arena bounds
    G.player1_z = clamp(
        G.player1_z,
        -G.arena_half_length + half_len,
        +G.arena_half_length - half_len
    );

    G.player2_z = clamp(
        G.player2_z,
        -G.arena_half_length + half_len,
        +G.arena_half_length - half_len
    );

}

// loads wall and floor textures using sfml
static void loadTextures() {
    static sf::Texture wallTexture;
    if (!wallTexture.loadFromFile("wall_texture.jpg")) {
        std::cerr << "Failed to load wall texture\n";
    }
    wallTexture.setRepeated(true);
    wallTexture.setSmooth(true);

    T.wallTexID = wallTexture.getNativeHandle();

    static sf::Texture floorTexture;
    if (!floorTexture.loadFromFile("floor_texture.jpg")) {
        std::cerr << "Failed to load floor texture\n";
    }
    floorTexture.setRepeated(true);
    floorTexture.setSmooth(true);
    floorTexture.generateMipmap();
    T.floorTexID = floorTexture.getNativeHandle();
}

// draws all 3D objects in the scene
static void drawScene(float dt) {
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
    setupView();

    // ball
    glPushMatrix();
    glTranslatef(G.ball_pos.x, G.ball_pos.y, G.ball_pos.z);
    drawBall(0.6);
    glPopMatrix();
    
    // paddles
	drawPaddle(G.player1_x, G.player1_z,                    // position
		G.paddle_length, G.paddle_width, G.paddle_height,   // dimensions
		0.9490f, 0.1843f, 0.1843f);                         // color
	drawPaddle(G.player2_x, G.player2_z, 
        G.paddle_length, G.paddle_width, G.paddle_height,
        0.1843f, 0.5137f, 0.9490f);
	
    // floor
    glEnable(GL_TEXTURE_2D);
    glBindTexture(GL_TEXTURE_2D, T.floorTexID);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST_MIPMAP_NEAREST);
    drawCuboidTex(
		-G.arena_half_width - G.paddle_length/4.0, G.arena_half_width + G.paddle_length / 4.0, // X
        0.0f, -0.1f,                                                                           // Y
		-G.arena_half_length, G.arena_half_length,                                             // Z
		0.0588f, 0.5412f, 0.1882f                                                              // Color
    );
	// walls
    glEnable(GL_TEXTURE_2D);
	glBindTexture(GL_TEXTURE_2D, T.wallTexID);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);

	drawCuboidTex(
        -G.arena_half_width - G.paddle_width, G.arena_half_width + G.paddle_width,
        2.0f, 0.0f,
        -G.arena_half_length, -G.arena_half_length - 2.0f,
        0.9098f, 0.7059f, 0.4431f);
	drawCuboidTex(
        -G.arena_half_width - G.paddle_width, G.arena_half_width + G.paddle_width,
        2.0f, 0.0f,
        G.arena_half_length + 2.0f, G.arena_half_length,
        0.9098f, 0.7059f, 0.4431f);
    drawCuboidTex(
        -G.arena_half_width - G.paddle_width - 1.0f, -G.arena_half_width - 1.0f,
        2.0f, 0.0f,
        G.arena_half_length + 2.0f, -G.arena_half_length - 2.0f,
		0.9098f, 0.7059f, 0.4431f);
    drawCuboidTex(
        G.arena_half_width + G.paddle_width - 1.0f, G.arena_half_width + G.paddle_width + 1.0f,
        2.0f, 0.0f,
		G.arena_half_length + 2.0f, -G.arena_half_length - 2.0f,
		0.9098f, 0.7059f, 0.4431f);
	glDisable(GL_TEXTURE_2D);

    // lines
	drawLine(-0.1f, 0.1f, -G.arena_half_length, G.arena_half_length);
	drawLine(-G.arena_half_width - 0.1f, -G.arena_half_width + 0.1f, -G.arena_half_length, G.arena_half_length);
	drawLine(G.arena_half_width - 0.1f, G.arena_half_width + 0.1f, -G.arena_half_length, G.arena_half_length);
}



int main() {
	// configure SFML window with OpenGL context
    sf::ContextSettings cs; cs.depthBits = 24; cs.stencilBits = 8; cs.majorVersion = 2; cs.minorVersion = 1;
    sf::RenderWindow win(sf::VideoMode(1024, 768), "Pong 3D", sf::Style::Default, cs);
    win.setVerticalSyncEnabled(true);
    win.setActive(true);

    setupLight();
    initOpenGL();
    setupProjection(win.getSize());
    initQuadric();

    sf::Clock clock;

    loadTextures();

    std::cout
        << "Controls:\n"
		<< "  Player 1: Left Arrow / Right Arrow\n"
		<< "  Player 2: A / D\n"
		<< "  C      : toggle top-down view\n"
        << "  Esc    : exit\n";

	// seed random number generator
    srand(static_cast<unsigned>(time(nullptr)));
    GL.round_active = false;
    GL.round_timer = 2.0f;

    bool running = true;
    while (running) {
        const float dt = clock.restart().asSeconds();

		// event handling
        for (sf::Event e; win.pollEvent(e); ) {
            if (e.type == sf::Event::Closed) running = false;
            if (e.type == sf::Event::KeyPressed && e.key.code == sf::Keyboard::Escape) running = false;
			if (e.type == sf::Event::KeyPressed && e.key.code == sf::Keyboard::C) G.top_down_view = !G.top_down_view;

            if (e.type == sf::Event::Resized)
                setupProjection(win.getSize());
        }

        handleInput(dt);

        gameUpdate(dt);

        if (checkGameOver())
            running = false;

        drawScene(dt);
        win.display();
    }

    freeQuadric();
    return 0;
}