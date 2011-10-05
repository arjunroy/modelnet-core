/*
 * Copyright (c) 2003 Duke University  All rights reserved.
 * See COPYING for license statement.
 */

#include <boost/config.hpp>
#include <iostream>
#include <vector>
#include <map>
#include <utility>
#include <algorithm>
#include <string>
#include <boost/graph/adjacency_list.hpp>


#include <xercesc/util/PlatformUtils.hpp>
#include <xercesc/parsers/AbstractDOMParser.hpp>
#include <xercesc/dom/DOMImplementation.hpp>
#include <xercesc/dom/DOMImplementationLS.hpp>
#include <xercesc/dom/DOMImplementationRegistry.hpp>
#include <xercesc/dom/DOMLSParser.hpp> // Formerly DOMBuilder.hpp
#include <xercesc/dom/DOMException.hpp>
#include <xercesc/dom/DOMDocument.hpp>
#include <xercesc/dom/DOMNodeList.hpp>
#include <xercesc/dom/DOMError.hpp>
#include <xercesc/dom/DOMLocator.hpp>
#include <xercesc/dom/DOMNamedNodeMap.hpp>

#include <xercesc/dom/DOMErrorHandler.hpp>


#define StrX(x) XMLString::transcode(x)

using namespace boost;
using namespace std;
#if XERCES_VERSION_MAJOR==2 && XERCES_VERSION_MINOR>1
using namespace xercesc;
#else
using namespace XERCES_CPP_NAMESPACE;
#endif

#include "mngraph.h"

// ---------------------------------------------------------------------------
//  Simple error handler deriviative to install on parser
// ---------------------------------------------------------------------------
class DOMCountErrorHandler : public DOMErrorHandler
{
public:
    // -----------------------------------------------------------------------
    //  Constructors and Destructor
    // -----------------------------------------------------------------------
    DOMCountErrorHandler();
    ~DOMCountErrorHandler();


    // -----------------------------------------------------------------------
    //  Getter methods
    // -----------------------------------------------------------------------
    bool getSawErrors() const;


    // -----------------------------------------------------------------------
    //  Implementation of the DOM ErrorHandler interface
    // -----------------------------------------------------------------------
    bool handleError(const DOMError& domError);
    void resetErrors();


private :
    // -----------------------------------------------------------------------
    //  Unimplemented constructors and operators
    // -----------------------------------------------------------------------
    DOMCountErrorHandler(const DOMCountErrorHandler&);
    void operator=(const DOMCountErrorHandler&);


    // -----------------------------------------------------------------------
    //  Private data members
    //
    //  fSawErrors
    //      This is set if we get any errors, and is queryable via a getter
    //      method. Its used by the main code to suppress output if there are
    //      errors.
    // -----------------------------------------------------------------------
    bool    fSawErrors;
};

void setAttrs(AttributeMap &mnattrmap, DOMNamedNodeMap *attrmap)
{
    unsigned int len = attrmap->getLength();
    for (unsigned int j = 0; j < len; j++) {
	const char *name = StrX(attrmap->item(j)->getNodeName());
	const char *value = StrX(attrmap->item(j)->getNodeValue());
	if (strncmp(name,"int_",4)==0) {
		char *end;
		mnattrmap.i(name+4) = strtoul(value,&end,0);
		if (end==value) cerr << "int conversion failed for" <<					name << ": " << value << endl;
	} else if (strncmp(name,"dbl_",4)==0) {
		char *end;
		mnattrmap.d(name+4) = strtod(value,&end);
		if (end==value) cerr << "dbl conversion failed for" <<					name << ": " << value << endl;
	} else 
		mnattrmap.s(name) = value;

    }
}


MNGraph*
docToGraph(DOMDocument *doc)
{
    if (!doc) return NULL;

    DOMNode *rootNode = (DOMNode*)doc->getDocumentElement();
    if (!rootNode) return NULL;
    char *rootname = StrX(rootNode->getNodeName());
    if (strcmp(rootname,"topology")) {
	    cerr << "Root element not topology: " << rootname << endl;
	return NULL;
	}

    DOMNodeList *vertextags = doc->getElementsByTagName(StrX("vertex"));
    DOMNodeList *edgetags = doc->getElementsByTagName(StrX("edge"));

    MNGraph *g = new MNGraph(vertextags->getLength());

    MNGraph::VertexIterator vertex, v_end;
    tie(vertex,v_end) = vertices(*g);
    for(int i=0; vertex != v_end; ++i,++vertex) {
	    DOMNamedNodeMap *attrmap = vertextags->item(i)->getAttributes();
	    setAttrs((*g)[*vertex],attrmap);
    }

    int edges = edgetags->getLength();
    boost::property_map<MNGraph, edge_weight_t>::type weightmap = get(edge_weight, *g);
    for(int i=0; i < edges; ++i) {
	    DOMNamedNodeMap *attrmap = edgetags->item(i)->getAttributes();
	    int src = XMLString::parseInt(
		    attrmap->getNamedItem(StrX("int_src"))->getNodeValue() );
	    int dst = XMLString::parseInt(
		    attrmap->getNamedItem(StrX("int_dst"))->getNodeValue() );
            MNGraph::Edge e = add_edge(src,dst,*g).first;
            setAttrs((*g)[e],attrmap);
	    weightmap[e] = 1;
    }
    setAttrs((*g),rootNode->getAttributes());

    return g;
}

MNGraph*
readgraph(const char *graphfile)
{
    // Initialize the XML4C2 system
    try
    {
         XMLPlatformUtils::Initialize();
    }

    catch (const XMLException& toCatch)
    {
         cerr << "Error during initialization! :\n"
              << StrX(toCatch.getMessage()) << endl;
         return NULL;
    }

    
    // Instantiate the DOM parser.
    static const XMLCh gLS[] = { chLatin_L, chLatin_S, chNull };
    DOMImplementation *impl = DOMImplementationRegistry::getDOMImplementation(gLS);
    //DOMBuilder        *parser = ((DOMImplementationLS*)impl)->createDOMBuilder(DOMImplementationLS::MODE_SYNCHRONOUS, 0);
    DOMLSParser *parser = ((DOMImplementationLS*)impl)->createLSParser(DOMImplementationLS::MODE_SYNCHRONOUS, NULL);
    //
    // And create our error handler and install it
    DOMCountErrorHandler errorHandler;
    //parser->setErrorHandler(&errorHandler);

    DOMDocument *doc = 0;

    try
    {
        doc = parser->parseURI(graphfile);
    }
    catch (const XMLException& toCatch)
    {
	    cerr << "\nError during parsing: '" << graphfile << "'\n"
                 << "Exception message is:  \n"
                 << StrX(toCatch.getMessage()) << "\n" << endl;
    }
    catch (...)
    {
	    cerr << "\nUnexpected exception during parsing: '" << graphfile
		    << "'\n";
    }

    MNGraph *g = NULL;
    if (doc) {
	    g = docToGraph(doc);
    }
    else {
	    cerr << "no graph document in " << graphfile << endl;
	    return NULL;
    }

    //
    //  Delete the parser itself.  Must be done prior to calling Terminate, below.
    //
    parser->release();


    // And call the termination method
    XMLPlatformUtils::Terminate();

    return g;
}



DOMCountErrorHandler::DOMCountErrorHandler() :

    fSawErrors(false)
{
}

DOMCountErrorHandler::~DOMCountErrorHandler()
{
}


// ---------------------------------------------------------------------------
//  DOMCountHandlers: Overrides of the DOM ErrorHandler interface
// ---------------------------------------------------------------------------
bool DOMCountErrorHandler::handleError(const DOMError& domError)
{
    fSawErrors = true;
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

void DOMCountErrorHandler::resetErrors()
{
    fSawErrors = false;
}
