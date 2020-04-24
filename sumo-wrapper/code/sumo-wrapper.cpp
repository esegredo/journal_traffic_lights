#include <iostream>
#include <string>
#include <fstream>
#include <vector>
#include <cstdlib>
#include <ctime>
#include <sstream>
#include <map>
#include <chrono>
#include <sys/wait.h>
#include "cInstance.hpp"
#include "simpleXMLParser.hpp"

using namespace std;

typedef struct
{
	double 		GvR; 					// Original Green vs Red
	double 		nGvR; 				// Normalized GvR
	double 		duration;			// Total duration
	unsigned 	numVeh;		 		// Vehicles arriving
	unsigned 	remVeh; 			// Vehicles not arriving
	double		stops;				// Number of stops (waiting counts and not planned stops)
	double		waitingTime;	// Total time that vehicles have been waiting (at a speed lower than 0.1 m/s)
	double		fitness;			// Fitness
	// New stats
	double		meanTravelTime;		// Mean Travel Time
	double		meanWaitingTime;	// Mean Waiting Time
	long double	CO2;						// CO2 
	long double	CO;							// CO
	long double	HC;					 		// HC
	long double	NOx;						// NOx
	long double	PMx;						// PMx
	long double	fuel;						// fuel
	long double	noise;					// noise
} tStatistics;

// Auxiliar functions


// Convert a number to string
// string to_string(unsigned long v);

// Build XML additional file with new tlLogic tags from TL configuration
void buildXMLfile(const cInstance &c, const vector<unsigned> &tl_times, unsigned long long t, string dir);

// Deletes the created files
void deleteFiles(const cInstance &c, unsigned long long t, string dir);

// Read TL configuration (generated by the algorithm)
void readTLtime(string tl_filename, vector<unsigned> &tl);

// Build command for executing SUMO
string buildCommand(const cInstance &c, unsigned long long t, string dir);

// Write the result file 
void writeResults(const tStatistics &s, string filename);

// Executes a command with a pipe
string execCommandPipe(string command);


// Calculating statistics
// Calculate GvR value
void calculateGvR(const cInstance &c, const vector<unsigned> & tl_times, tStatistics &s);

// Analyze trip info obtaining how many vehicle arriving, number of stops, total duration, ...
void analyzeTripInfo(const cInstance &c, unsigned long long t, tStatistics &s, string dir);

// Analyze Summary File
void analyzeSummary(const cInstance &c, unsigned long long t, tStatistics &s, string dir);

// Calculate Fitness
double calculateFitness(const tStatistics &s, unsigned simTime);

// Analyze emission file
void analyzeEmissions(const cInstance &c, unsigned long long t, tStatistics &s, string dir);

int main(int argc, char **argv)
{
	cInstance instance;
	tStatistics s;
	vector<unsigned> tl_times;

	//time_t current_time = time(0), t2, t3, t4;
  auto start = chrono::high_resolution_clock::now();
  auto current_time = chrono::duration_cast<chrono::nanoseconds>(start.time_since_epoch()).count();

  string cmd;

	if(argc != 6)
	{
		cout << "Usage: " << argv[0] << " <instance_file> <dir> <traffic light configuration> <result file> <delete generated files>" << endl;
		exit(-1);
	}

	instance.read(argv[1]);
  
	readTLtime(argv[3], tl_times);

	buildXMLfile(instance, tl_times, current_time, argv[2]);

	cmd = buildCommand(instance, current_time, argv[2]);

	cout << "Executing sumo ..." << endl;

	auto t2 = chrono::high_resolution_clock::now();
	execCommandPipe(cmd);
	auto t3 = chrono::duration_cast<chrono::milliseconds>(chrono::high_resolution_clock::now() - t2).count();

	cout << "Obtaining statistics ..." << endl;

	// Obtaining statistics (JM):
	calculateGvR(instance, tl_times, s);
	analyzeTripInfo(instance, current_time, s, argv[2]);
	analyzeSummary(instance, current_time, s, argv[2]);
	//analyzeEmissions(*instance, current_time, s); 
	s.fitness = calculateFitness(s, instance.getSimulationTime());

	writeResults(s, argv[4]);
	auto t4 = chrono::duration_cast<chrono::milliseconds>(chrono::high_resolution_clock::now() - start).count();

	cout << "Total time: " << t4 << endl;
	cout << "SUMO time: " << t3 << endl;
	cout << endl;

  if (stoi(argv[5]) == 1)
    deleteFiles(instance, current_time, argv[2]);

	return 0;
}

// Convert a number to string
/*string to_string(unsigned long v)
{
	stringstream ss;
	ss << v;

	return ss.str();
}*/

void buildXMLfile(const cInstance &c, const vector<unsigned> &tl_times, unsigned long long t, string dir)
{  
//	string xmlfile = c.getPath() + dir + "/" + to_string(t) + "-" + c.getName() + ".add.xml";
	string xmlfile = c.getPath() + dir + "/" + c.getName() + ".add.xml";
	ofstream fout_xml(xmlfile.c_str());
	unsigned nTL = c.getNumberOfTLlogics();
	vector<string> phases;
	unsigned nPhases;
	unsigned tl_times_counter = 0;

	fout_xml << "<additional>" << endl;

	for (int i=0;i<nTL;i++)
	{
		//fout_xml << "\t<tlLogic id=\"" << c.getTLID(i) << "\" type=\"static\" programID=\"1\" offset=\"0\">" << endl;
    fout_xml << "\t<tlLogic id=\"" << c.getTLID(i) << "\" type=\"static\" programID=\"1\" offset=\"" << tl_times[tl_times_counter] << "\">" << endl;
    tl_times_counter++;
		phases = c.getPhases(i);
		nPhases = phases.size();

		for (int j=0;j<nPhases;j++)
		{
			fout_xml << "\t\t<phase duration=\"" << tl_times[tl_times_counter] << "\" state=\"" << phases[j] << "\"/>" << endl;
			tl_times_counter++;
		}
		fout_xml << "\t</tlLogic>" << endl;
	}
	fout_xml << "</additional>" << endl;

	fout_xml.close();
}

void deleteFiles(const cInstance &c, unsigned long long t, string dir) {
//  string baseName = c.getPath() + dir + "/" + to_string(t) + "-" + c.getName();
  string baseName = c.getPath() + dir + "/" + c.getName();

  string cmd = "unlink " + baseName + ".add.xml";
	execCommandPipe(cmd);

  cmd = "unlink " + baseName + "-summary.xml";
	execCommandPipe(cmd);
  
  cmd = "unlink " + baseName + "-tripinfo.xml";
	execCommandPipe(cmd);
  
  cmd = "unlink " + baseName + "-vehicles.xml";
	execCommandPipe(cmd);
}

void readTLtime(string tl_filename, vector<unsigned> &tl)
{
	ifstream fin_tl(tl_filename.c_str());
	unsigned t;

	fin_tl >> t;
	while(!fin_tl.eof())
	{
		tl.push_back(t);
		fin_tl >> t;
	}
	fin_tl.close();	
}

string buildCommand(const cInstance &c, unsigned long long t, string dir)
{
	//string cmd = "sumo ";
	string cmd = "sumo -W "; 
	string name1 = c.getPath() + c.getName(); // Required instace files
  //string name2 = c.getPath() + dir + "/" + to_string(t) + "-" + c.getName(); // generated files by this application
	string name2 = c.getPath() + dir + "/" + c.getName(); // generated files by this application

	// Input files:
	cmd += "-n " + name1 + ".net.xml "; // Network file
	cmd += "-r " + name1 + ".rou.xml "; // Routes file
	cmd += "-a " + name2 + ".add.xml "; // TL file 

	// Output files:
  //cmd += "--save-configuration " + name2 + ".cfg "; // Save configuration <= With this option configuration file is generated but no SUMO execution is performed
  //cmd += "--emission-output " + name1 + "-emissions.xml "; // Emissions result
	cmd += "--summary-output " + name2 + "-summary.xml "; // Summary result
	cmd += "--vehroute-output " + name2 + "-vehicles.xml "; // Vehicle routes result
	cmd += "--tripinfo-output " + name2 + "-tripinfo.xml "; // tripinfo result

	// Options:
	cmd += "-b 0 "; // Begin time
	cmd += "-e " + to_string(c.getSimulationTime()) + " "; // End time
	cmd += "-s 0 "; // route steps (with value 0, loads the whole route file)
	cmd += "--time-to-teleport -1 "; // Disable teleporting
	cmd += "--no-step-log "; // Disable console output
	//cmd += "--device.hbefa.probability 1.0 "; // Tripinfo file will include emissions stats	
  cmd += "--device.emissions.probability 1.0 ";
  //cmd += "--seed " + to_string(t); // random seed
	cmd += "--seed 23432 ";

  // THIS IS NEW!!!! No validation
  cmd += "--xml-validation never";

	return cmd;
}

// GvR (Green vs Red)
// Requires tl configuration (no sumo simulation required)

// GvR = \sum_{i=0}^{totalPhases}{GvR_phase(i)}
// GvR_phase(i) = duration(i) * (number_of_greens(i) / number_of_reds(i))
// Larger values are better
// Disadvantages: - No normalized - yellow/red phases are not counted
// Alternatives: 
// Normaziled GvR: nGvR
// nGVR = 1/number_of_TL * \sum_{i = 0}^{number_of_TL}{ (\sum_{j=0}^{phases_TL(i)}{GvR_phase(j)})/ (\sum_{j=0}^{phases_TL(i)}{duration(j)}) }
// Larger values are better
// Advantages: - Normalized (0/1) - All phases are taken into account
void calculateGvR(const cInstance &c, const vector<unsigned> & tl_times, tStatistics &s)
{
	unsigned nTL = c.getNumberOfTLlogics(); // Number of TL logics
	unsigned nPhases; // Number of phases of a TL
	unsigned nt; // Number of tl in a TL logic

	// auxiliar variables
	vector<string> phases;
	unsigned tl_times_counter = 0;
	double gvr, dur, red, green;
	
	s.GvR = s.nGvR = 0;
	for (int i=0;i<nTL;i++)
	{
		phases = c.getPhases(i);
		nPhases = phases.size();
		tl_times_counter++;
		
		gvr = 0;
		dur = 0;

		for (int j=0;j<nPhases;j++)
		{
			red = green = 0;
			nt = phases[j].size();
			for(int k = 0; k < nt; k++)
			{
				if(phases[j][k] == 'r') red++;
				else if(toupper(phases[j][k]) == 'G') green++;
			}

			if(red == 0) red++;

			gvr += (green/red)*tl_times[tl_times_counter];
			dur += tl_times[tl_times_counter];
			tl_times_counter++;
		}
		s.GvR += gvr;
		s.nGvR += (gvr/dur);
	}
	s.nGvR /= nTL;
}

void analyzeTripInfo(const cInstance &c, unsigned long long t, tStatistics &s, string dir)
{
//	string filename = c.getPath() + dir + "/" + to_string(t) + "-" + c.getName() + "-tripinfo.xml";
	string filename = c.getPath() + dir + "/" + c.getName() + "-tripinfo.xml";

	string line;
	ifstream fin(filename.c_str());
	map<string, string> m;
	int position;

	s.numVeh = 0;
	s.duration = 0;
	s.stops = 0;
	s.waitingTime = 0;
	s.CO2 = 0;
	s.CO = 0;
	s.HC = 0;
	s.NOx = 0;
	s.PMx = 0;
	s.fuel = 0;
	s.noise = 0;

	getline(fin, line);
	while(!fin.eof())
	{
		if(isSubString(line,"id=",position))
		{
			// get map
			getPairMap(line, m);
	
			s.numVeh++;
			s.duration += atof(m["duration"].c_str()); 
      // It is the number of times vehicles have been waiting (at a speed lower than 0.1 m/s)
      // It does not include the number of planned stops
			s.stops += atof(m["waitingCount"].c_str());
      // It is the times a vehicle has been waiting (at a speed lower than 0.1 m/s)
      // It does not include the planned stops time
			s.waitingTime += atof(m["waitingTime"].c_str());
		}
		else if(isSubString(line,"CO_abs=",position))
		{
			getPairMap(line, m);
	
			s.CO2 += atof(m["CO2_abs"].c_str());
			s.CO += atof(m["CO_abs"].c_str());
			s.HC += atof(m["HC_abs"].c_str());
			s.NOx += atof(m["NOx_abs"].c_str());
			s.PMx += atof(m["PMx_abs"].c_str());
			s.fuel += atof(m["fuel_abs"].c_str());
			//s.noise += atof(m["noise"].c_str());
		}

		getline(fin, line);
	}

	s.remVeh = c.getNumberOfVehicles() - s.numVeh;
	fin.close();
}

void analyzeSummary(const cInstance &c, unsigned long long t, tStatistics &s, string dir)
{
//	string filename = c.getPath() + dir + "/" + to_string(t) + "-" + c.getName() + "-summary.xml";
	string filename = c.getPath() + dir + "/" + c.getName() + "-summary.xml";

	string line,last_line;
	ifstream fin(filename.c_str());
	map<string, string> m;
	int position;

	getline(fin, line);
	while(!fin.eof())
	{
		if(isSubString(line,"time=",position))
		{
			last_line = line;
		}
		getline(fin, line);
	}

	// get map
	getPairMap(last_line, m);

	s.meanTravelTime = atof(m["meanTravelTime"].c_str()); 
	s.meanWaitingTime = atof(m["meanWaitingTime"].c_str()); 

	fin.close();
}

double calculateFitness(const tStatistics &s, unsigned simTime)
{
	return (s.duration + (s.remVeh * simTime) + s.waitingTime) / (s.numVeh * s.numVeh + s.GvR);
}

void analyzeEmissions(const cInstance &c, unsigned long long t, tStatistics &s, string dir)
{
//	string filename = c.getPath() + dir + "/" + to_string(t) + "-" + c.getName() + "-emissions.xml";
	string filename = c.getPath() + dir + "/" + c.getName() + "-emissions.xml";
	string line;
	ifstream fin(filename.c_str());
	map<string, string> m;
	int position;

	s.CO2 = 0;
	s.CO = 0;
	s.HC = 0;
	s.NOx = 0;
	s.PMx = 0;
	s.fuel = 0;
	s.noise = 0;

	getline(fin, line);
	while(!fin.eof())
	{
		if(isSubString(line,"id=",position))
		{
			// get map
			getPairMap(line, m);
	
			s.CO2 += atof(m["CO2"].c_str());
			s.CO += atof(m["CO"].c_str());
			s.HC += atof(m["HC"].c_str());
			s.NOx += atof(m["NOx"].c_str());
			s.PMx += atof(m["PMx"].c_str());
			s.fuel += atof(m["fuel"].c_str());
			s.noise += atof(m["noise"].c_str());
		}
		getline(fin, line);
	}

	fin.close();
}

void writeResults(const tStatistics &s, string filename)
{
	ofstream fout(filename.c_str());

	fout << s.GvR << " // Original Green vs Red" << endl;
	fout << s.nGvR << " // Normalized GvR" << endl;
	fout << s.duration << " // Total duration" << endl;
	fout << s.numVeh << " // Vehicles arriving" << endl;
	fout << s.remVeh << " // Vehicles not arriving" << endl;
	fout << s.stops << " // Number of stops (waiting counts)" << endl;
	fout << s.waitingTime << " // Total waiting time (at a speed lower than 0.1 m/s)" << endl;
	fout << s.fitness << " // Fitness" << endl;
	fout << s.meanTravelTime << " // Mean Travel Time" << endl;	
	fout << s.meanWaitingTime << " // Mean Waiting Time" << endl;
	fout << s.CO2 << " // CO2 " << endl;	
	fout << s.CO << " // CO" << endl;	
	fout << s.HC << " // HC" << endl;	
	fout << s.NOx << " // NOx" << endl;	
	fout << s.PMx << " // PMx" << endl;	
	fout << s.fuel << " // fuel" << endl;	
	fout << s.noise << " // noise" << endl;	
	fout.close();
}

string execCommandPipe(string command) {
  const int MAX_BUFFER_SIZE = 128;
  FILE* pipe = popen(command.c_str(), "r");

  // Waits until the execution ends
  if (wait(NULL) == -1){
    //cerr << "Error waiting for simulator results" << endl;
    return "Error waiting for simulator results";
  }

  char buffer[MAX_BUFFER_SIZE];
  string result = "";
  while (!feof(pipe)) {
    if (fgets(buffer, MAX_BUFFER_SIZE, pipe) != NULL)
      result += buffer;
  }
  pclose(pipe);
  return result;
}
