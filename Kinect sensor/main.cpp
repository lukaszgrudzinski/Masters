
#include "main.h"
#include "glut.h"

#include <cmath>
#include <cstdio>

#include <Windows.h>
#include <Ole2.h>

#include <Kinect.h>
#include <fstream>

// We'll be using buffer objects to store the kinect point cloud
GLuint vboId;
GLuint cboId;

// Intermediate Buffers
unsigned char rgbimage[colorwidth*colorheight*4];    // Stores RGB color image
ColorSpacePoint depth2rgb[width*height];             // Maps depth pixels to rgb pixels
CameraSpacePoint depth2xyz[width*height];			 // Maps depth pixels to 3d coordinates

// Body tracking variables
BOOLEAN tracked;							// Whether we see a body
Joint joints[JointType_Count];				// List of joints in the tracked body

// Kinect Variables
IKinectSensor* sensor;             // Kinect sensor
IMultiSourceFrameReader* reader;   // Kinect data source
ICoordinateMapper* mapper;         // Converts between depth, color, and 3d coordinates

bool initKinect() {
    if (FAILED(GetDefaultKinectSensor(&sensor))) {
		return false;
	}
	if (sensor) {
		sensor->get_CoordinateMapper(&mapper);

		sensor->Open();
		sensor->OpenMultiSourceFrameReader(
			FrameSourceTypes::FrameSourceTypes_Depth | FrameSourceTypes::FrameSourceTypes_Color | FrameSourceTypes::FrameSourceTypes_Body,
			&reader);
		return reader;
	} else {
		return false;
	}
}

void getDepthData(IMultiSourceFrame* frame, GLubyte* dest) {
	IDepthFrame* depthframe;
	IDepthFrameReference* frameref = NULL;
	frame->get_DepthFrameReference(&frameref);
	frameref->AcquireFrame(&depthframe);
	if (frameref) frameref->Release();

	if (!depthframe) return;

	// Get data from frame
	unsigned int sz;
	unsigned short* buf;
	static int count = 0;
	depthframe->AccessUnderlyingBuffer(&sz, &buf);
	std::ofstream myfile;
	myfile.open("frameD", std::ios_base::app);

	// Write vertex coordinates
	mapper->MapDepthFrameToCameraSpace(width*height, buf, width*height, depth2xyz);
	float* fdest = (float*)dest;
	for (int i = 0; i < sz; i++) {
		*fdest++ = depth2xyz[i].X;
		*fdest++ = depth2xyz[i].Y;
		*fdest++ = depth2xyz[i].Z;
		if (count < 20)
		myfile << depth2xyz[i].X << "," << depth2xyz[i].Y << ',' << depth2xyz[i].Z << '\n';
	}
	if (count < 20)
		myfile << ':';
	myfile.close();
	count++;
	// Fill in depth2rgb map
	mapper->MapDepthFrameToColorSpace(width*height, buf, width*height, depth2rgb);
	if (depthframe) depthframe->Release();
}

void getRgbData(IMultiSourceFrame* frame, GLubyte* dest) {
	static int count = 0;
	IColorFrame* colorframe;
	IColorFrameReference* frameref = NULL;
	frame->get_ColorFrameReference(&frameref);
	frameref->AcquireFrame(&colorframe);
	if (frameref) frameref->Release();

	if (!colorframe) return;

	// Get data from frame
	colorframe->CopyConvertedFrameDataToArray(colorwidth*colorheight*4, rgbimage, ColorImageFormat_Rgba);

	// Write color array for vertices
	float* fdest = (float*)dest;

	std::ofstream myfile;
	myfile.open("frameC", std::ios_base::app);
	for (int i = 0; i < width*height; i++) {
		ColorSpacePoint p = depth2rgb[i];
		// Check if color pixel coordinates are in bounds
		if (p.X < 0 || p.Y < 0 || p.X > colorwidth || p.Y > colorheight) {
			*fdest++ = 0;
			*fdest++ = 0;
			*fdest++ = 0;
		}
		else {
			int idx = (int)p.X + colorwidth*(int)p.Y;
			*fdest++ = rgbimage[4*idx + 0]/255.;
			*fdest++ = rgbimage[4*idx + 1]/255.;
			*fdest++ = rgbimage[4*idx + 2]/255.;
			if(count<20)
			myfile<<rgbimage[4 * idx + 0] / 255.<<","<< rgbimage[4 * idx + 1] / 255.<<','<< rgbimage[4 * idx + 2] / 255.<<'\n';
		}
    }
	if (count < 20)
	myfile << ":";
	myfile.close();
	count++;
	if (colorframe) colorframe->Release();
}


void saveRGBFrame(double r,double g, double b, std::ofstream file)
{
	file << r<<","<<g<<b<<"\n";
}

void getBodyData(IMultiSourceFrame* frame) {
	IBodyFrame* bodyframe;
	IBodyFrameReference* frameref = NULL;
	frame->get_BodyFrameReference(&frameref);
	frameref->AcquireFrame(&bodyframe);
	if (frameref) frameref->Release();

	if (!bodyframe) return;

	IBody* body[BODY_COUNT] = { 0 };
	bodyframe->GetAndRefreshBodyData(BODY_COUNT, body);
	for (int i = 0; i < BODY_COUNT; i++) {
		body[i]->get_IsTracked(&tracked);
		if (tracked) {
			body[i]->GetJoints(JointType_Count, joints);
			break;
		}
	}

	if (bodyframe) bodyframe->Release();
}

void getKinectData() {
	IMultiSourceFrame* frame = NULL;
	if (SUCCEEDED(reader->AcquireLatestFrame(&frame))) {
		GLubyte* ptr;
		glBindBuffer(GL_ARRAY_BUFFER, vboId);
		ptr = (GLubyte*)glMapBuffer(GL_ARRAY_BUFFER, GL_WRITE_ONLY);
		if (ptr) {
			getDepthData(frame, ptr);
		}
		glUnmapBuffer(GL_ARRAY_BUFFER);
		glBindBuffer(GL_ARRAY_BUFFER, cboId);
		ptr = (GLubyte*)glMapBuffer(GL_ARRAY_BUFFER, GL_WRITE_ONLY);
		if (ptr) {
			getRgbData(frame, ptr);
		}
		glUnmapBuffer(GL_ARRAY_BUFFER);
        getBodyData(frame);
	}
	if (frame)
	{

		frame->Release();
	}
}

void rotateCamera() {
	static double angle = 0.;
	static double radius = 3.;
	double x = radius*sin(angle);
	double z = radius*(1-cos(angle)) - radius/2;
	glMatrixMode(GL_MODELVIEW);
    glLoadIdentity();
	gluLookAt(x,0,z,0,0,radius/2,0,1,0);
	//angle += 0.001;
}

void drawKinectData() {
	getKinectData();
	rotateCamera();

	glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
	glEnableClientState(GL_VERTEX_ARRAY);
	glEnableClientState(GL_COLOR_ARRAY);

	glBindBuffer(GL_ARRAY_BUFFER, vboId);
	glVertexPointer(3, GL_FLOAT, 0, NULL);

	glBindBuffer(GL_ARRAY_BUFFER, cboId);
	glColorPointer(3, GL_FLOAT, 0, NULL);

	glPointSize(1.f);
	glDrawArrays(GL_POINTS, 0, width*height);

	glDisableClientState(GL_VERTEX_ARRAY);
	glDisableClientState(GL_COLOR_ARRAY);

	if (tracked) {
		// Draw some arms
		const CameraSpacePoint& lh = joints[JointType_WristLeft].Position;
		const CameraSpacePoint& le = joints[JointType_ElbowLeft].Position;;
		const CameraSpacePoint& ls = joints[JointType_ShoulderLeft].Position;;
		const CameraSpacePoint& rh = joints[JointType_WristRight].Position;;
		const CameraSpacePoint& re = joints[JointType_ElbowRight].Position;;
		const CameraSpacePoint& rs = joints[JointType_ShoulderRight].Position;;
		glBegin(GL_LINES);
		glColor3f(1.f, 0.f, 0.f);
		glVertex3f(lh.X, lh.Y, lh.Z);
		glVertex3f(le.X, le.Y, le.Z);
		glVertex3f(le.X, le.Y, le.Z);
		glVertex3f(ls.X, ls.Y, ls.Z);
		glVertex3f(rh.X, rh.Y, rh.Z);
		glVertex3f(re.X, re.Y, re.Z);
		glVertex3f(re.X, re.Y, re.Z);
		glVertex3f(rs.X, rs.Y, rs.Z);
		glEnd();
	}
}

int main(int argc, char* argv[]) {
    if (!init(argc, argv)) return 1;
    if (!initKinect()) return 1;

    // OpenGL setup
    glClearColor(0,0,0,0);
    glClearDepth(1.0f);

	// Set up array buffers
	const int dataSize = width*height * 3 * 4;
	glGenBuffers(1, &vboId);
	glBindBuffer(GL_ARRAY_BUFFER, vboId);
	glBufferData(GL_ARRAY_BUFFER, dataSize, 0, GL_DYNAMIC_DRAW);
	glGenBuffers(1, &cboId);
	glBindBuffer(GL_ARRAY_BUFFER, cboId);
	glBufferData(GL_ARRAY_BUFFER, dataSize, 0, GL_DYNAMIC_DRAW);

    // Camera setup
    glViewport(0, 0, width, height);
    glMatrixMode(GL_PROJECTION);
    glLoadIdentity();
	gluPerspective(45, width /(GLdouble) height, 0.1, 1000);
    glMatrixMode(GL_MODELVIEW);
    glLoadIdentity();
	gluLookAt(0,0,0,0,0,1,0,1,0);

    // Main loop
    execute();
    return 0;
}
