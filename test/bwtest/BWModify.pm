package BWModify;

# Changes the bandwidth attribute under spec tag in  bwtest.model file

use English;
use XML::DOM;
use IO::File;
use base 'Exporter';

use strict;
use warnings;

our @EXPORT = qw (change_modelfile_bw);

my $link_type = 'client-client';
my $bw_attr_name = 'dbl_kbps';

# Changes bandwidth (in kbps) in modelfile
# TODO: Actual error handling. For now, it always returns true.
sub change_modelfile_bw {
    (my $modelfile_name, my $hopbw_kbps) = @_;
    my $parser=new XML::DOM::Parser;
    my $doc=$parser->parsefile($modelfile_name);

    my $nodes=$doc->getElementsByTagName($link_type);
    my $node= $nodes->item(0);

    $node->setAttribute($bw_attr_name, "$hopbw_kbps");
    $doc->printToFile($modelfile_name);
    $doc->dispose;
    return 1;
}

