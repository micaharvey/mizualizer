//-----------------------------------------------------------------------------
// name: Mizualizer.cpp
// desc: Music Visualizer
//
// author: Micah Arvey (marvey@ccrma.stanford.edu)
//   date: fall 2014
//   uses: RtAudio by Gary Scavone
//         Open GL
//-----------------------------------------------------------------------------
#include "RtAudio.h"
#include "chuck_fft.h"
#include "x-vector3d.h"
#include "x-fun.h"
#include <math.h>
#include <stdlib.h>
#include <iostream>

#ifdef __MACOSX_CORE__
#include <GLUT/glut.h>
#include <OpenGL/gl.h>
#include <OpenGL/glu.h>
#else
#include <GL/gl.h>
#include <GL/glu.h>
#include <GL/glut.h>
#endif

using namespace std;



//-----------------------------------------------------------------------------
// function prototypes
//-----------------------------------------------------------------------------
void initGfx();
void idleFunc();
void displayFunc();
void reshapeFunc( GLsizei width, GLsizei height );
void keyboardFunc( unsigned char, int, int );
void mouseFunc( int button, int state, int x, int y );
void specialInput(int key, int x, int y);
void handleParams( int argc, char ** argv );
void printUsage( );

// our datetype
#define SAMPLE float
// corresponding format for RtAudio
#define MY_FORMAT RTAUDIO_FLOAT32
// sample rate
#define MY_SRATE 44100
// number of channels
#define MY_CHANNELS 1
// for convenience
#define MY_PIE 3.14159265358979

// width, height, and position
long g_width = 1024;
long g_height = 720;
long g_last_width = g_width;
long g_last_height = g_height;
int g_x_position = 350;
int g_y_position = 100;
// global buffer
SAMPLE * g_buffer = NULL;
// fft buffer
SAMPLE * g_fftBuf = NULL;
long g_bufferSize;
// window
SAMPLE * g_window = NULL;
long g_windowSize;
//waterfall
unsigned int g_numDroplets = 64;
//complex ** g_waterfallArr = complex*[g_numDroplets];
complex ** g_waterfallArr = NULL;

// light 0 position
GLfloat g_light0_pos[4] = { 2.0f, 8.2f, 4.0f, 1.0f };

// global variables
GLboolean g_fullscreen = FALSE;
bool g_draw_dB = false;
bool g_displayWaterfall = true;
bool g_rainbowWaterfall = false;
long g_drawWindowWidth = 0;
GLfloat g_barrel_roll = 0.0f;
bool g_minefield = false;
// global time variables
int g_t = 0;
int g_t2 = 0;
GLfloat g_color = 0;


//-----------------------------------------------------------------------------
// name: callme()
// desc: audio callback
//-----------------------------------------------------------------------------
int callme( void * outputBuffer, void * inputBuffer, unsigned int numFrames,
            double streamTime, RtAudioStreamStatus status, void * data )
{
    // cast!
    SAMPLE * input = (SAMPLE *)inputBuffer;
    SAMPLE * output = (SAMPLE *)outputBuffer;
    
    // fill
    for( int i = 0; i < numFrames; i++ )
    {
        // assume mono
        g_buffer[i] = input[i];
        // zero output
        output[i] = 0;
    }
    
    return 0;
}

//-----------------------------------------------------------------------------
// Name: handleParams( int argc, char ** argv );
// Desc: called to handle command line options, sets the gvars
//-----------------------------------------------------------------------------
void handleParams( int argc, char ** argv ){



}

//-----------------------------------------------------------------------------
// Name: printUsage( );
// Desc: prints usage
//-----------------------------------------------------------------------------
void printUsage(){
    cerr << "----------------------------------------------------" << endl;
    cerr << "Mizualizer(0xdecaf)" << endl;
    cerr << "Micah Arvey" << endl;
    cerr << "https://ccrma.stanford.edu/people/micah-arvey" << endl;
    cerr << "----------------------------------------------------" << endl;
    cerr << "'h' - print this help message" << endl;
    cerr << "'q' - quit visualization" << endl;
    cerr << "'d' - reset defaults" << endl;
    cerr << "'s' - toggle fullscreen" << endl;
    cerr << "'w' - toggle waterfall" << endl;
    cerr << "'r' - toggle rainbowz" << endl;
    cerr << "'z' - makes the spectrum over lower frequencies" << endl;
    cerr << "'x' - makes the spectrum over higher frequencies" << endl;
    cerr << "'l/r arrow' - do a barrel roll!" << endl; 
    cerr << "'m' - places mines in the field" << endl;
    cerr << "----------------------------------------------------" << endl;
}



//-----------------------------------------------------------------------------
// name: main()
// desc: entry point
//-----------------------------------------------------------------------------
int main( int argc, char ** argv ){
    // print usage
    printUsage();
    // handle our parameters
    handleParams( argc, argv );

    // instantiate RtAudio object
    RtAudio audio;
    // variables
    unsigned int bufferBytes = 0;
    // frame size
    unsigned int bufferFrames = 2048;
    
    // check for audio devices
    if( audio.getDeviceCount() < 1 )
    {
        // nopes
        cout << "no audio devices found!" << endl;
        exit( 1 );
    }
    
    // initialize GLUT
    glutInit( &argc, argv );
    // init gfx
    initGfx();
    
    // let RtAudio print messages to stderr.
    audio.showWarnings( true );
    
    // set input and output parameters
    RtAudio::StreamParameters iParams, oParams;
    iParams.deviceId = audio.getDefaultInputDevice();
    iParams.nChannels = MY_CHANNELS;
    iParams.firstChannel = 0;
    oParams.deviceId = audio.getDefaultOutputDevice();
    oParams.nChannels = 2;
    oParams.firstChannel = 0;
    
    // create stream options
    RtAudio::StreamOptions options;

    // go for it
    try {
        // open a stream
        audio.openStream( &oParams, &iParams, MY_FORMAT, MY_SRATE, &bufferFrames, &callme, (void *)&bufferBytes, &options );
    }
    catch( RtError& e )
    {
        // error!
        cout << e.getMessage() << endl;
        exit( 1 );
    }
    
    // compute
    bufferBytes = bufferFrames * MY_CHANNELS * sizeof(SAMPLE);
    // allocate global buffer
    g_bufferSize = bufferFrames;
    g_buffer = new SAMPLE[g_bufferSize];
    g_fftBuf = new SAMPLE[g_bufferSize];
    memset( g_buffer, 0, sizeof(SAMPLE)*g_bufferSize );
    memset( g_fftBuf, 0, sizeof(SAMPLE)*g_bufferSize );

    // allocate buffer to hold window
    g_windowSize = bufferFrames;
    g_window = new SAMPLE[g_windowSize];
    // generate the window
    hanning( g_window, g_windowSize );

    // allows us to draw only the lower frequencies
    g_drawWindowWidth = g_windowSize/2;

    //waterfall buffer setup
    g_waterfallArr = new complex*[g_numDroplets];
    for(int i = 0; i < g_numDroplets; i++){
        g_waterfallArr[i] = new complex[g_bufferSize];
        memset( g_waterfallArr[i], 0, sizeof(complex)*g_bufferSize );
    }

    // go for it
    try {
        // start stream
        audio.startStream();
        
        // let GLUT handle the current thread from here
        glutMainLoop();
        
        // stop the stream.
        audio.stopStream();
    }
    catch( RtError& e )
    {
        // print error message
        cout << e.getMessage() << endl;
        goto cleanup;
    }
    
cleanup:
    // close if open
    if( audio.isStreamOpen() )
        audio.closeStream();
    
    // done
    return 0;
}




//-----------------------------------------------------------------------------
// Name: reshapeFunc( )
// Desc: called when window size changes
//-----------------------------------------------------------------------------
void initGfx()
{
    // double buffer, use rgb color, enable depth buffer
    glutInitDisplayMode( GLUT_DOUBLE | GLUT_RGB | GLUT_DEPTH );
    // initialize the window size
    glutInitWindowSize( g_width, g_height );
    // set the window postion
    glutInitWindowPosition( g_x_position, g_y_position );
    // create the window
    glutCreateWindow( "Mizualizer" );
    
    // set the idle function - called when idle
    glutIdleFunc( idleFunc );
    // set the display function - called when redrawing
    glutDisplayFunc( displayFunc );
    // set the reshape function - called when client area changes
    glutReshapeFunc( reshapeFunc );
    // set the keyboard function - called on keyboard events
    glutKeyboardFunc( keyboardFunc );
    // set the mouse function - called on mouse stuff
    glutMouseFunc( mouseFunc );
    // specifically for the arrow keys
    glutSpecialFunc( specialInput );
    
    // set clear color
    glClearColor( 0, 0, 0, 1 );
    // enable color material
    glEnable( GL_COLOR_MATERIAL );
    // enable depth test
    glEnable( GL_DEPTH_TEST );
    // set the front faces of polygons
    glFrontFace( GL_CCW );
    // draw in wireframe
    glPolygonMode( GL_FRONT_AND_BACK, GL_FILL );
    // enable normalize
    glEnable( GL_NORMALIZE );
    
    // enable lighting
    //glEnable( GL_LIGHTING );
    // enable lighting for front
    glLightModeli( GL_FRONT_AND_BACK, GL_TRUE );
    // material have diffuse and ambient lighting
    glColorMaterial( GL_FRONT_AND_BACK, GL_AMBIENT_AND_DIFFUSE );
    // enable color
    glEnable( GL_COLOR_MATERIAL );

    // enable light 0
    glEnable( GL_LIGHT0 );    
    // set the position of the lights
    glLightfv( GL_LIGHT0, GL_POSITION, g_light0_pos );

}




//-----------------------------------------------------------------------------
// Name: reshapeFunc( )
// Desc: called when window size changes
//-----------------------------------------------------------------------------
void reshapeFunc( GLsizei w, GLsizei h )
{
    // save the new window size
    g_width = w; g_height = h;
    // map the view port to the client area
    glViewport( 0, 0, w, h );
    // set the matrix mode to project
    glMatrixMode( GL_PROJECTION );
    // load the identity matrix
    glLoadIdentity( );
    // create the viewing frustum
    gluPerspective( 45.0, (GLfloat) w / (GLfloat) h, 1.0, 300.0 );
    // set the matrix mode to modelview
    glMatrixMode( GL_MODELVIEW );
    // load the identity matrix
    glLoadIdentity( );
    // position the view point
    gluLookAt( 0.0f, 0.0f, 10.0f, 0.0f, 0.0f, 0.0f, 0.0f, 1.0f, 0.0f );
}




//-----------------------------------------------------------------------------
// Name: keyboardFunc( )
// Desc: key event
//-----------------------------------------------------------------------------
void keyboardFunc( unsigned char key, int x, int y )
{
    switch( key )
    {
        case 'Q':
        case 'q':
            exit(1);
            break;
        case 'H':
        case 'h':
            printUsage();
            break;
        case 'D':
        case 'd':
            g_draw_dB = !g_draw_dB;
            g_fullscreen = false;
            g_displayWaterfall = true;
            g_rainbowWaterfall = false;
            g_drawWindowWidth = g_windowSize/2;
            g_barrel_roll = 0.0f;
            glLoadIdentity( );
            gluLookAt( 0.0f, 0.0f, 10.0f, 0.0f, 0.0f, 0.0f, 0.0f, 1.0f, 0.0f );
            glutReshapeWindow( g_last_width, g_last_height );
            glutPositionWindow(g_x_position, g_y_position);
            break;
        case 'S': // toggle fullscreen
        case 's':
        {
            // check fullscreen
            if( !g_fullscreen )
            {
                g_last_width = g_width;
                g_last_height = g_height;
                glutFullScreen();
            }
            else{
                glutReshapeWindow( g_last_width, g_last_height );
                glutPositionWindow(g_x_position, g_y_position);
            }
            
            // toggle variable value
            g_fullscreen = !g_fullscreen;
            break;
        }
        case 'W':
        case 'w':
            g_displayWaterfall = !g_displayWaterfall;
            break;
        case 'R':
        case 'r':
            g_rainbowWaterfall = !g_rainbowWaterfall;
            break;
        case 'Z':
        case 'z':
            if(g_drawWindowWidth > 100)
                g_drawWindowWidth -= 64; 
            break;
        case 'X':
        case 'x':
            if (g_drawWindowWidth < g_windowSize/2)
                g_drawWindowWidth += 64; 
            break;
        case 'M':
        case 'm':
            cerr << "mines: " << g_minefield << endl;
            g_minefield = !g_minefield;
            break;
    }
    
    glutPostRedisplay( );
}

void specialInput(int key, int x, int y)
{
    switch(key){
        case GLUT_KEY_UP:
            break;  
        case GLUT_KEY_DOWN:
            //do something here
            break;
        case GLUT_KEY_LEFT:
            if(g_barrel_roll > -2.0)
                g_barrel_roll -= 0.1f;
            // load the identity matrix
            glLoadIdentity( );
            // position the view point
            gluLookAt( 0.0f, 0.0f, 10.0f, 0.0f, 0.0f, 0.0f, g_barrel_roll, 1.0f, 0.0f );
            break;
        case GLUT_KEY_RIGHT:
            if(g_barrel_roll < 2.0)
                g_barrel_roll += 0.1f;
            // load the identity matrix
            glLoadIdentity( );
            // position the view point
            gluLookAt( 0.0f, 0.0f, 10.0f, 0.0f, 0.0f, 0.0f, g_barrel_roll, 1.0f, 0.0f );
            break;
    }
}

//-----------------------------------------------------------------------------
// Name: mouseFunc( )
// Desc: handles mouse stuff
//-----------------------------------------------------------------------------
void mouseFunc( int button, int state, int x, int y )
{
    if( button == GLUT_LEFT_BUTTON )
    {
        // when left mouse button is down
        if( state == GLUT_DOWN )
        {
        }
        else
        {
        }
    }
    else if ( button == GLUT_RIGHT_BUTTON )
    {
        // when right mouse button down
        if( state == GLUT_DOWN )
        {
        }
        else
        {
        }
    }
    else
    {
    }
    
    glutPostRedisplay( );
}




//-----------------------------------------------------------------------------
// Name: idleFunc( )
// Desc: callback from GLUT
//-----------------------------------------------------------------------------
void idleFunc( )
{
    // render the scene
    glutPostRedisplay( );
}



//-----------------------------------------------------------------------------
// Name: displayFunc( )
// Desc: callback function invoked to draw the client area
//-----------------------------------------------------------------------------
void displayFunc( )
{
    // local state
    static GLfloat zrot = 0.0f, c = 0.0f;
    int colorStep = 0;
    
    // clear the color and depth buffers
    glClear( GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT );

    // line width
    glLineWidth( 1.0 );
    // define a starting point
    GLfloat x = -5;
    // increment
    GLfloat xinc = ::fabs(x*2 / g_bufferSize);

    // color
    glColor3f( .5, 1, .5 );
    if(g_rainbowWaterfall){
        glColor3f( sin(MY_PIE*g_color), 
                   sin(MY_PIE*g_color+MY_PIE/2), 
                   sin(MY_PIE*g_color+MY_PIE*3/2) );
    }

    // apply window
    apply_window( g_buffer, g_window, g_windowSize );

    // save original transformation state ( OT state )
    glPushMatrix();
        glTranslatef(0, 2, 0);
        glBegin( GL_LINE_STRIP );
            // loop over buffer
            for( int i = 0; i < g_bufferSize; i++ )
            {
                // plot
                glVertex2f( x, 3*g_buffer[i] );
                x += xinc;
            }
        glEnd();
    //Pop OTS
    glPopMatrix();

    // copy into the fft buf
    memcpy( g_fftBuf, g_buffer, sizeof(SAMPLE)*g_bufferSize );
    // apply window to buf
    apply_window( g_fftBuf, g_window, g_windowSize );
    // take forward FFT (time domain signal -> frequency domain signal)
    rfft( g_fftBuf, g_windowSize/2, FFT_FORWARD );
    // cast the result to a buffer of complex values (re,im)
    complex * cbuf = (complex *)g_fftBuf;
    
    /* THIS IS BIG */
    // update our waterfallArr
    for(int i = g_numDroplets-1; i > 0; i--){
        g_waterfallArr[i] = g_waterfallArr[i-1];
    }
    complex* frequencyBuf = new complex[g_bufferSize];
    memcpy( frequencyBuf, cbuf, sizeof(complex)*g_bufferSize );
    g_waterfallArr[0] = frequencyBuf;
    /* HACKY BABY */

    // define a starting point
    x = -5;
    // compute increment
    xinc = ::fabs(x*2 / (g_drawWindowWidth));

    // color of cbuf (complex*) g_fftBuf see above cast
    glColor3f( .5, 1, .5 );

    // more waterfall
    for(int i_waterfall = 0; i_waterfall < g_numDroplets; i_waterfall++){
        if( i_waterfall > 0 && !g_displayWaterfall)
            continue;
        // redefine starting point
        x = -5;
        // save transformation state
        glPushMatrix();
            // translate
            glTranslatef( 0, (-2 + (i_waterfall*0.04)), (0 - (i_waterfall*0.2)) );
            // color of waterfall
            if( g_rainbowWaterfall ){
                colorStep = 2*MY_PIE*i_waterfall/g_numDroplets;
                glColor3f( (sin(colorStep+MY_PIE/2)+1)/2.0, (sin(colorStep)+1)/2.0, (sin(colorStep-MY_PIE/2)+1)/2.0 );
            }
            // start primitive
            glBegin( GL_LINE_STRIP );
                // loop over buffer
                for( int i = 0; i < g_drawWindowWidth; i++)
                {
                    // plot the magnitude with scaling, and also "compression" via pow(...)
                    glVertex2f( x, 10*pow( cmp_abs(g_waterfallArr[i_waterfall][i]), .5 ) );
                    // increment vars
                    x += xinc;
                }
            // end primitive
            glEnd();
        // pop
        glPopMatrix();
    }

    g_color += 0.01f;

    // flush!
    glFlush( );
    // swap the double buffer
    glutSwapBuffers( );
}



