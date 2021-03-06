///only move sematic, not redundant
//https://docs.microsoft.com/en-gb/cpp/cpp/explicitly-defaulted-and-deleted-functions
//http://blog.matejzavrsnik.com/using_smart_pointers_in_building_trees_in_which_child_nodes_need_an_access_to_the_parent.html
//http://www.opengl-tutorial.org/miscellaneous/clicking-on-objects/picking-with-custom-ray-obb-function/

//#include "stdafx.h"
#include <igl/opengl/glfw/Viewer.h>
#include <igl/jet.h>
#include <igl/igl_inline.h>
#include <igl/per_vertex_normals.h>
#include <igl/ply.h>
#include <igl/readPLY.h>
#include <igl/readMESH.h>
#include <igl/readOBJ.h>
#include <igl/readOFF.h>

#include <iostream>
#include <Eigen/Dense>
#include <string>
#include <array>

#include "tutorial_shared_path.h"
#include "octree.h"
#include "testing.h"

using namespace Eigen;
using namespace std;

template <class T>
vector<array<T, 3>> Eigen2CPP(Matrix<T, Dynamic, 3> & mat) {
	/*
	This function serves as a bridge between Eigen and this C++ header project.
	Input: an Eigen matrix, that NEEDS to be dynamic rows and with 3 cols. The type is templated.
	Output: a C++ vector of arrays with 3 columns. The type is templated.
	*/
	T* T_ptr = mat.data();
	vector<T> col0(T_ptr, T_ptr + mat.rows());
	vector<T> col1(T_ptr + mat.rows(), T_ptr + 2 * mat.rows());
	vector<T> col2(T_ptr + 2 * mat.rows(), T_ptr + 3 * mat.rows());
	vector<array<T, 3>> out;
	for (size_t i = 0; i < mat.rows(); ++i) {
		out.push_back({ col0.at(i), col1.at(i), col2.at(i) });
	}
	return out;
}

int main(int argc, char *argv[])
{
	//Some testing routines
	//test_RayBox();
	//test_RayTriangle();
	//test_Eigen2CPP();
	//test_build();

	//Read data
	Matrix<double, Dynamic,3> V;
	Matrix<int, Dynamic, 3> F;
	Matrix<double, Dynamic, 3> N;
	Matrix<double, Dynamic, 3> B;
	Matrix<double, Dynamic, 3> temp;

	igl::readOBJ(TUTORIAL_SHARED_PATH "/armadillo.obj", V, F);
	//igl::readOBJ(TUTORIAL_SHARED_PATH "/bumpy-cube.obj", V, F);
	//igl::readOBJ(TUTORIAL_SHARED_PATH "/arm.obj", V, F);
	//igl::readMESH(TUTORIAL_SHARED_PATH "/octopus-high.mesh", V, temp,F);
	//igl::readMESH(TUTORIAL_SHARED_PATH "/big-sigcat.mesh", V, temp, F);
	//igl::readMESH(TUTORIAL_SHARED_PATH "/hand.mesh", V, temp, F);
	//igl::readOFF(TUTORIAL_SHARED_PATH "/fertility.off", V, F);
	//igl::readOFF(TUTORIAL_SHARED_PATH "/lion.off", V, F);
	//igl::readOFF(TUTORIAL_SHARED_PATH "/cheburashka.off", V, F);
	//igl::readOFF(TUTORIAL_SHARED_PATH "/camelhead.off", V, F);
	//igl::readOFF(TUTORIAL_SHARED_PATH "/bunny.off", V, F);

	//Compute the vertex normals with the help of libigl
	igl::per_vertex_normals(V, F, N);
	//Compute the barycenters of the faces with the help of libigl
	igl::barycenter(V, F, B);
	
	cout << "Welcome to octreeSDF, an octree implementation of the Surface Diameter Function." << endl;
	cout << "This is still under development, TODO: very big meshes, visualization." << endl;
	cout << "The mesh has V.rows(): " << V.rows() << ", F.rows(): " << F.rows() << ", B.rows():" << B.rows() << endl;
	cout << endl;

	vector<array<double,3>> vecV = Eigen2CPP(V);
	vector<array<int, 3>> vecF = Eigen2CPP(F);
	vector<array<double, 3>> vecN = Eigen2CPP(N);
	vector<array<double, 3>> vecB = Eigen2CPP(B);

	//This is just a benchmarking routine here. We want the average computation time. 
	double total1 = 0;
	for (int i = 0; i < 30; ++i) {
		chrono::high_resolution_clock::time_point t1 = chrono::high_resolution_clock::now();
		SDF tree(vecV, vecF, vecB);
		tree.init();
		tree.build();
		chrono::high_resolution_clock::time_point t2 = chrono::high_resolution_clock::now();
		chrono::duration<double> time_span = chrono::duration_cast<chrono::duration<double>>(t2 - t1);
		total1 += time_span.count();
	}
	total1 /= 30;
	cout << "Average running time (30 trials) for tree.build() is " << total1 << " seconds." << endl;
	cout << endl;

	//Main part, doing the SDF queries. 
	//We need the normals to look inward, not outward, w.r.t. the surface.
	//Doing the queries
	for (size_t i = 0; i < vecN.size(); ++i) {
		vecN.at(i)[0] = -vecN.at(i)[0];
		vecN.at(i)[1] = -vecN.at(i)[1];
		vecN.at(i)[2] = -vecN.at(i)[2];
	}

	//Create the search tree
	SDF tree(vecV, vecF, vecB);
	tree.init();
	tree.build();
	//Compute the sdf queries
	vector<double> sdf = tree.query(vecN);

	//Adjust the values for visualization
	//Raw, the results of the sdf are too small for the visualiser to pick up on them
	double mymax = *max_element(sdf.begin(), sdf.end());
	double mean = 0;
	for (size_t i = 0; i < sdf.size(); ++i) {
		mean += sdf.at(i);
	}
	mean /= sdf.size();
	for (size_t i = 0; i < sdf.size(); ++i) {
		//This adjustment seems to work for most meshes
		sdf.at(i) = log(sdf.at(i) +  mean/ mymax); //for octopus, sig-cat, fertility, hand mesh, lion, cheburashka, camelhead

	}

	//Creating the visualisation
	double* ptr = &sdf[0];
	Map<VectorXd> Z(ptr, sdf.size());
	MatrixXd C(V.rows(), 3);
	igl::jet(Z, true, C);

	//// Plot the mesh
	igl::opengl::glfw::Viewer viewer;
	viewer.data().set_mesh(V, F);
	//// Launch the viewer

	viewer.data().set_colors(C);
	viewer.data().show_lines = !viewer.data().show_lines;
	viewer.launch();
}