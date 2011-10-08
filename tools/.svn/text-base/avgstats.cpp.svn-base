#include <iostream>
#include <iomanip>
#include <fstream>
#include <strstream>
#include <vector>
#include <string>

using namespace std;

bool readVals(vector<ifstream*>& files, vector<vector<double> >& vals)
{
    int i;
    for (i=0; i< files.size(); i++)
    {
	string line;
	if (!getline(*files[i],line))
	    return false;
	istrstream in(line.c_str());
	double v;
	vals[i].clear();
	while (in >> v)
	    vals[i].push_back(v);
    }
    return i > 0;
}

int main(int argc, char* argv[])
{
    unsigned int i;
    vector<string> filenames(argv+1, argv+argc);
    vector<ifstream*> files;
    for (i=0;i<filenames.size();i++)
	files.push_back(new ifstream(filenames[i].c_str()));
    vector<vector<double> > vals(files.size());
    ofstream outavg("avgs.txt");
    ofstream outmax("maxs.txt");
    while (readVals(files,vals))
    {
	for (unsigned int col=0;col<vals[0].size();col++)
	{
	    double sum = 0, m = 0;
	    unsigned int file;
	    for (file=0;  file< vals.size(); file++)
	    {
		sum += vals[file][col];
		if (vals[file][col] > m)
		    m = vals[file][col];
	    }
	    outavg << setw(14) << sum/file <<" ";
	    outmax << setw(14) << m << " ";
	}
	outavg << endl;
	outmax << endl;
    }
    for (i=0;i<files.size();i++)
    {
	files[i]->close();
	delete files[i];
    }
    return 0;
}
