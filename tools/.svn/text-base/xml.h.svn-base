#ifndef __XML_H_
#define __XML_H_


#include <vector>
#include <map>
#include <string>
using namespace std;

class XMLElement {
 public:
  XMLElement();
  XMLElement(const string& name, const string& content="");
  XMLElement(const XMLElement& other);
  void setName(const string& name);
  void setText(const string& text);
  const string& getName() const;
  const string& getText() const;
  const string& getAttribute(const string& key) const;
  void setAttribute(const string& key, const string& val);
  bool hasAttribute(const string& key) const;
  const map<string,string>& getAttributes() const;
  vector<const XMLElement*> findChildrenByName(const string& name) const;
  vector<XMLElement>& getChildren();
  const vector<XMLElement>& getChildren() const;
  void addChild(const XMLElement& child);
  void removeChild(int i);
  void write(ostream& out) const;
 private:
  void write(ostream& out, const string& indent) const;
  void findChildrenByName(const string& name, vector<const XMLElement*>& results) const;
  string myName;
  string myContent;
  vector<XMLElement> myChildren;
  map<string, string> myAttributes;
};

class XMLStream 
{
public:
  virtual ~XMLStream(){}
  virtual void onElement(const string& name, map<string,string>& attrs){}
  virtual void onElementEnd(const string& name){}
  virtual void onError(const string& msg){}
};

bool readXML(const string& filename, XMLElement& root);
bool streamXML(const string& filename, XMLStream& stream);
#endif
