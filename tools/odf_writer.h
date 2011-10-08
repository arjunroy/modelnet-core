/* $Id: odf_writer.h 143 2003-05-22 17:01:29Z eade $ */

/*
 * A writer for odf file format (Otter Data Format)
 * from a LEDA graph representation.
 *
 * This writer currently only preserves top-level node and edge attributes,
 * since the odf file format is kind of bizarre and difficult.
 *
 * For a description of the odf format, take a look at:
 *     modelnet/docs/formats.notes
 */


bool odf_write(const mn_graph &g, ostream &out);
bool odf_write(const mn_graph &g, const char *filename);

