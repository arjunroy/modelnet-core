#include <iostream>
#include <xercesc/util/PlatformUtils.hpp>
#include <xercesc/parsers/AbstractDOMParser.hpp>
#include <xercesc/dom/DOMImplementation.hpp>
#include <xercesc/dom/DOMImplementationLS.hpp>
#include <xercesc/dom/DOMImplementationRegistry.hpp>
#include <xercesc/dom/DOMBuilder.hpp>
#include <xercesc/dom/DOMException.hpp>
#include <xercesc/dom/DOMDocument.hpp>
#include <xercesc/dom/DOMNodeList.hpp>
#include <xercesc/dom/DOMError.hpp>
#include <xercesc/dom/DOMLocator.hpp>
#include <xercesc/dom/DOMNamedNodeMap.hpp>
#include <xercesc/dom/DOMErrorHandler.hpp>
#include <xercesc/parsers/SAXParser.hpp>
#include <xercesc/sax/HandlerBase.hpp>
#include <xercesc/util/XMLString.hpp>
#include "xml.h"

#define StrX(x) XMLString::transcode(x)

using namespace std;
#ifdef __FreeBSD__
using namespace xercesc;
#endif


class DOMCountErrorHandler : public DOMErrorHandler
{
public:
  DOMCountErrorHandler() : sawErrors(false) {}
  ~DOMCountErrorHandler(){}

  bool getSawErrors() const {return sawErrors;}
  bool handleError(const DOMError& domError) {
    sawErrors = true;
    if (domError.getSeverity() == DOMError::DOM_SEVERITY_WARNING)
      cerr << "\nWarning at file ";
    else if (domError.getSeverity() == DOMError::DOM_SEVERITY_ERROR)
      cerr << "\nError at file ";
    else
      cerr << "\nFatal Error at file ";
    
    cerr << StrX(domError.getLocation()->getURI())
         << ", line " << domError.getLocation()->getLineNumber()
         << ", char " << domError.getLocation()->getColumnNumber()
         << "\n  Message: " << StrX(domError.getMessage()) << endl;
    
    return false;
  }
  void resetErrors() { sawErrors = false; }
private :
  bool sawErrors;
};

pair<DOMDocument*, DOMBuilder*> readXML(const string& filename) 
{
  try {
    XMLPlatformUtils::Initialize();
  }

  catch (const XMLException& toCatch) {
    cerr << "Error during initialization! :\n"
	 << StrX(toCatch.getMessage()) << endl;
    return make_pair((DOMDocument*)0,(DOMBuilder*)0);
  }
  
  // Instantiate the DOM parser.
  static const XMLCh gLS[] = { chLatin_L, chLatin_S, chNull };
  DOMImplementation *impl = DOMImplementationRegistry::getDOMImplementation(gLS);
  DOMBuilder *parser = ((DOMImplementationLS*)impl)->createDOMBuilder(DOMImplementationLS::MODE_SYNCHRONOUS, 0);
  //
  // And create our error handler and install it
  DOMCountErrorHandler errorHandler;
  parser->setErrorHandler(&errorHandler);

  DOMDocument *doc = 0;
  
  try {
    doc = parser->parseURI(filename.c_str());
  }
  catch (const XMLException& toCatch) {
    cerr << "\nError during parsing: '" << filename << "'\n"
	 << "Exception message is:  \n"
	 << StrX(toCatch.getMessage()) << endl << endl;
    return make_pair(doc,parser);
  }
  catch (...) {
    cerr << "\nUnexpected exception during parsing: '" << filename << "'\n";
    return make_pair(doc,parser);
  }
  if (!doc) {
    cerr << "no graph document in " << filename << endl;
  }
  return make_pair(doc, parser);
}

XMLElement::XMLElement()
{
}

XMLElement::XMLElement(const string& name, const string& content="") : myName(name), myContent(content)
{
}

XMLElement::XMLElement(const XMLElement& other)
{
  myChildren = other.myChildren;
  myAttributes = other.myAttributes;
  myName = other.myName;
  myContent = other.myContent;
}

void XMLElement::setName(const string& name)
{
  myName = name;
}

void XMLElement::setText(const string& text)
{
  myContent = text;
}

const string& XMLElement::getName() const
{
  return myName;
}

const string& XMLElement::getText() const
{
  return myContent;
}

const string& XMLElement::getAttribute(const string& key) const
{
  static string dummy;
  return myAttributes.count(key) ? myAttributes.find(key)->second : dummy;
}

void XMLElement::setAttribute(const string& key, const string& val)
{
  myAttributes[key]=val;
}

bool XMLElement::hasAttribute(const string& key) const
{
  return myAttributes.count(key);
}

const map<string,string>& XMLElement::getAttributes() const
{
  return myAttributes;
}

vector<const XMLElement*> XMLElement::findChildrenByName(const string& name) const
{
  vector<const XMLElement*> results;
  findChildrenByName(name, results);
  return results;
}

void XMLElement::findChildrenByName(const string& name, vector<const XMLElement*>& results) const
{
  for (unsigned int i=0; i<myChildren.size(); i++) {
    if (myChildren[i].getName() == name)
      results.push_back(&myChildren[i]);
    myChildren[i].findChildrenByName(name, results);
  }
}

vector<XMLElement>& XMLElement::getChildren()
{
  return myChildren;
}

const vector<XMLElement>& XMLElement::getChildren() const
{
  return myChildren;
}

void XMLElement::addChild(const XMLElement& child)
{
  myChildren.push_back(child);
}

void XMLElement::removeChild(int i)
{
  myChildren.erase(myChildren.begin()+i);
}

void XMLElement::write(ostream& out) const 
{
  out << "<?xml version=\"1.0\" encoding=\"ISO-8859-1\"?>\n";
  write(out, "");
}

void XMLElement::write(ostream& out, const string& indent) const 
{
  out << indent << "<" << myName << " ";
  for(map<string,string>::const_iterator it=myAttributes.begin(); it!= myAttributes.end(); it++)
    out << it->first << "=\"" << it->second << "\" ";
  if (myContent.length() || myChildren.size())
    out << ">\n";
  else {
    out << "/>\n";
    return;
  }
  for (unsigned int i=0; i<myChildren.size();i++)
    myChildren[i].write(out, indent+"  ");
  out << indent << "</"<<myName<<">\n";
}

void DOMtoXMLElement(DOMNode* dom, XMLElement& xml)
{
  xml.setName(StrX(dom->getNodeName()));
  unsigned int i;
  string text;
  DOMNodeList* children = dom->getChildNodes();
  for (i=0;children && i<children->getLength();i++) {
    DOMNode* child = children->item(i);
    switch (child->getNodeType()) {
    case DOMNode::TEXT_NODE:
      text+=StrX(child->getNodeValue());
      break;
    case DOMNode::ELEMENT_NODE:
      {
	XMLElement childElement;
	DOMtoXMLElement(child, childElement);
	xml.addChild(childElement);
      }
      break;
    default:
      continue;
    }
  }
  xml.setText(text);
  DOMNamedNodeMap* attrs = dom->getAttributes();
  if (attrs == 0)
    return;
  for (i=0;i<attrs->getLength();i++) {
    DOMNode* attr = attrs->item(i);
    xml.setAttribute(StrX(attr->getNodeName()), StrX(attr->getNodeValue()));
  }
}

bool readXML(const string& filename, XMLElement& root)
{
  pair<DOMDocument*, DOMBuilder*> doc = readXML(filename);
  if (!doc.first) {
    if (doc.second)
      doc.second->release();
    XMLPlatformUtils::Terminate();
    return false;
  }
  DOMtoXMLElement(doc.first, root);
  doc.second->release();
  XMLPlatformUtils::Terminate();
  return true;
}


class StreamHandler : public HandlerBase
{
 public:
  StreamHandler(XMLStream& stream) : myStream(stream)
  {
  }
  void startElement(const XMLCh* const name, AttributeList& attrList)
  {
    string label=XMLString::transcode(name);
    map<string,string> attrs;
    for (unsigned int i=0;i<attrList.getLength();i++)
      attrs[XMLString::transcode(attrList.getName(i))] = XMLString::transcode(attrList.getValue(i));
    myStream.onElement(label, attrs);
  }
  void endElement(const XMLCh* const name) 
  {
    myStream.onElementEnd(XMLString::transcode(name));
  }
  void fatalError(const SAXParseException& e)
  {
    myStream.onError(XMLString::transcode(e.getMessage()));
  }
private:
  XMLStream& myStream;
};

bool streamXML(const string& filename, XMLStream& stream)
{
  try {
    XMLPlatformUtils::Initialize();
  }
  catch (const XMLException& toCatch) {
    char* message = XMLString::transcode(toCatch.getMessage());
    cerr << "XML initialization failed: " << message << endl;
    return false;
  }
  SAXParser parser;
  parser.setDoValidation(true);    // optional.
  parser.setDoNamespaces(true);    // optional
  StreamHandler docHandler(stream);
  parser.setDocumentHandler(&docHandler);
  parser.setErrorHandler((ErrorHandler*)&docHandler);
  try {
    parser.parse(filename.c_str());
  }
  catch (const XMLException& toCatch) {
    char* message = XMLString::transcode(toCatch.getMessage());
    cerr << "XML Exception: "	<< message << endl;
    return false;
  }
  catch (const SAXParseException& toCatch) {
    char* message = XMLString::transcode(toCatch.getMessage());
    cerr << "XML Parsing exception: "	<< message << endl;
    return false;
  }
  catch (const SAXException& e) {
    char* message = XMLString::transcode(e.getMessage());
    cerr << "Bad file format exception: " << message << endl;
    return false;
  }
  catch (...) {
    cerr << "Unexpected Exception \n" ;
    return false;
  }
  return true;
}
