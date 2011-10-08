#include "mngraph.h"
#include <xercesc/parsers/SAXParser.hpp>
#include <xercesc/sax/HandlerBase.hpp>
#include <xercesc/util/XMLString.hpp>
#include <iostream>
#include <fstream>
#ifdef __FreeBSD__
using namespace xerces;
#endif

static string dummyStr;
static int dummyInt;
static double dummyDouble;

AttributeMap::AttributeMap()
{
}

AttributeMap::AttributeMap(const AttributeMap& other)
{
   myStrings = other.myStrings;
   myInts = other.myInts;
   myDoubles = other.myDoubles;
}

const string& AttributeMap::getString(const string& key) const
{
   return myStrings.count(key) ? myStrings.find(key)->second : dummyStr;
}

const int& AttributeMap::getInteger(const string& key) const
{
   return myInts.count(key) ? myInts.find(key)->second : dummyInt;
}

const double& AttributeMap::getDouble(const string& key) const
{
   return myDoubles.count(key) ? myDoubles.find(key)->second : dummyDouble;
}

void AttributeMap::setString(const string& key, const string& s)
{
   myStrings[key]=s;
}

void AttributeMap::setInteger(const string& key, const int& i)
{
   myInts[key]=i;
}

void AttributeMap::setDouble(const string& key, const double& d)
{
   myDoubles[key]=d;
}

bool AttributeMap::hasString(const string& key) const
{
   return myStrings.count(key);
}

bool AttributeMap::hasInteger(const string& key) const
{
   return myInts.count(key);
}

bool AttributeMap::hasDouble(const string& key) const
{
   return myDoubles.count(key);
}

AttributeMap& MNGraph::specs(const string& label)
{
  return mySpecs[label];
}

AttributeMap& MNGraph::specsFor(const edge& e)
{
  return (*this)[e].hasString("specs") ? specs((*this)[e].getString("specs")) : (*this)[e];
}

class MNGraphHandler : public HandlerBase
{
 public:
   MNGraphHandler(MNGraph& g) : myGraph(g)
     {
	myFailedFlag = false;
	inSpecs = false;
     }
   void startElement(const XMLCh* const name, AttributeList& attrs)
     {
	string label=XMLString::transcode(name);
	if (inSpecs)
	  addSpecs(label, attrs);
	else if (label == "vertex")
	  addVertex(attrs);
	else if (label == "edge")
	  addEdge(attrs);
	else if (label == "specs")
	  inSpecs = true;
     }
  void endElement(const XMLCh* const name) {
    if (inSpecs && string(XMLString::transcode(name))=="specs")
      inSpecs = false;
  }
  void fatalError(const SAXParseException& e)
  {
    cerr << XMLString::transcode(e.getMessage()) << endl;
    myFailedFlag = true;
  }
  bool failed() const 
  {
    return myFailedFlag;
  }
  
 private:
  void addAttribute(const string& name, const string& val, AttributeMap& am) {
    if (name.length() >= 4 && name.substr(0,4) == "int_")
      am.setInteger(name.substr(4), strtol(val.c_str(),0,10));
    else if (name.length() >= 4 && name.substr(0,4) == "dbl_")
      am.setDouble(name.substr(4), strtod(val.c_str(),0));
    else
      am.setString(name, val);
  }
  void addSpecs(const string& label, AttributeList& attrs) {
    unsigned int i;
    for (i=0;i<attrs.getLength();i++)
      {
	string name = XMLString::transcode(attrs.getName(i));
	string val = XMLString::transcode(attrs.getValue(i));
	addAttribute(name, val, myGraph.specs(label));
      }
  }
  void addVertex(AttributeList& attrs)
  {
    AttributeMap am;
    int idx=-1;
    unsigned int i;
    for (i=0;i<attrs.getLength();i++)
      {
	string name = XMLString::transcode(attrs.getName(i));
	string val = XMLString::transcode(attrs.getValue(i));
	if (name == "int_idx")
	  idx = strtol(val.c_str(),0,10);
	else 
	  addAttribute(name, val, am);
      }
    if (idx == -1) 
      {
	myFailedFlag=true;
	throw new SAXException("no vertex index");
      }
    while (myGraph.vertexCount() <= idx)
      myGraph.addVertex();
    myGraph[vertex(idx)]=am;
  }
  void addEdge(AttributeList& attrs)
  {
    AttributeMap am;
    int source=-1, target=-1;
    unsigned int i;
    for (i=0;i<attrs.getLength();i++)
      {
	string name = XMLString::transcode(attrs.getName(i));
	string val = XMLString::transcode(attrs.getValue(i));
	if (name == "int_src")
	  source = strtol(val.c_str(),0,10);
	else if (name == "int_dst")
	  target = strtol(val.c_str(),0,10);
	else if (name == "int_idx")
	  continue;
	else
	  addAttribute(name, val, am);
      }
    if (source == -1 || target == -1)
      {
	myFailedFlag = true;
	throw new SAXException("edge without source and target");
      }
    while (myGraph.vertexCount() <= (source > target ? source : target))
      myGraph.addVertex();
    edge e = myGraph.addEdge(vertex(source), vertex(target));
    myGraph[e]=am;
  }
  MNGraph& myGraph;
  bool inSpecs;
  bool myFailedFlag;
};

bool readGraphXML(const string& filename, MNGraph& g)
{
   try {
      XMLPlatformUtils::Initialize();
   }
   catch (const XMLException& toCatch) {
      char* message = XMLString::transcode(toCatch.getMessage());
      cerr << "XML initialization failed: " << message << endl;
      //XMLString::release(&message);
      return false;
   }
   SAXParser parser;
   parser.setDoValidation(true);    // optional.
   parser.setDoNamespaces(true);    // optional
   MNGraphHandler docHandler(g);
   parser.setDocumentHandler(&docHandler);
   parser.setErrorHandler((ErrorHandler*)&docHandler);
   try {
      parser.parse(filename.c_str());
   }
   catch (const XMLException& toCatch) {
      char* message = XMLString::transcode(toCatch.getMessage());
      cerr << "XML Exception: "	<< message << endl;
      //XMLString::release(&message);
      return false;
   }
   catch (const SAXParseException& toCatch) {
      char* message = XMLString::transcode(toCatch.getMessage());
      cerr << "XML Parsing exception: "	<< message << endl;
      //XMLString::release(&message);
      return false;
   }
   catch (const SAXException& e) {
      char* message = XMLString::transcode(e.getMessage());
      cerr << "Bad file format exception: " << message << endl;
      //XMLString::release(&message);
      return false;
   }
   catch (...) {
      cerr << "Unexpected Exception \n" ;
      return false;
   }
   return !docHandler.failed();
}

struct PrintAttrib {
   PrintAttrib(ostream& out, const string& prefix) : myOut(out), myPre(prefix){}
   template <class T>
     void operator()(AttributeMap& am, const string& name, const T& val) const
     {
	myOut << myPre << name << "= \"" << val << "\" ";
     }
   mutable ostream& myOut;
   const string& myPre;
};

void printAttribs(AttributeMap& am, ostream& out)
{
  am.forEachString(PrintAttrib(out,""));
  am.forEachInteger(PrintAttrib(out,"int_"));
  am.forEachDouble(PrintAttrib(out,"dbl_"));
}
  
struct PrintEdge {
   PrintEdge(ostream& out) : myOut(out){}
   void operator()(MNGraph::GraphType& g, const edge& e) const {
     myOut << "\t\t<edge int_src=\"" << g.source(e).index << "\" int_dst=\"" << g.target(e).index 
	   << "\" int_idx=\"" << e.index << "\" ";
     printAttribs(g[e], myOut);
     myOut << "/>\n";
   }
   mutable ostream& myOut;
};

struct PrintVertex {
   PrintVertex(ostream& out) : myOut(out){}
   void operator()(MNGraph::GraphType& g, const vertex& v) const
     {
	myOut << "\t\t<vertex int_idx=\"" << v.index << "\" ";
	printAttribs(g[v], myOut);
	myOut << "/>\n";
     }
   mutable ostream& myOut;
};

struct PrintSpecs {
  PrintSpecs(ostream& out) : myOut(out){}
   void operator()(const string& label, AttributeMap& am) const
     {
       myOut << "\t\t<" << label << " ";
       printAttribs(am, myOut);
       myOut << " />\n";
     }
   mutable ostream& myOut;
};

bool writeGraphXML(ostream& out, MNGraph& g)
{
   out << "<?xml version=\"1.0\" encoding=\"ISO-8859-1\"?>\n";
   out << "<topology>\n";
   out << "\t<vertices>\n";
   g.forEachVertex(PrintVertex(out));
   out << "\t</vertices>\n";
   out << "\t<edges>\n";
   g.forEachEdge(PrintEdge(out));
   out << "\t</edges>\n";
   out << "\t<specs>\n";
   g.forAllSpecs(PrintSpecs(out));
   out<< "\t</specs>\n";
   out << "</topology>\n";
   out.flush();
   return true;
}


  

