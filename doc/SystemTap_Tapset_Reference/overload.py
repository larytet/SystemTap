#! /usr/bin/python
# XML tree transformation for systemtap function overloading
# This script merges all overloaded tapset function entries
# into one entry.

import sys
from lxml import etree

DEBUG = 0

def collect_overloads(refentries):
    """
    Collect overloads into lists.
    """
    functions = {}
    for entry in refentries:
        name = entry.xpath("refnamediv/refname")[0].text
        if name not in functions:
            functions[name] = []
        functions[name].append(entry)
    return {k: v for (k, v) in functions.items() if len(v) > 1}

def get_params(functions):
    """
    Return a parameter list containing the parameters for all overloads.
    """
    seen = set()
    params = []
    for overload in functions:
        refsect = overload.xpath("refsect1[1]")[0]
        # add variablelist node for future construction
        if len(refsect.xpath("variablelist")) == 0:
            refsect.remove(refsect[1])
            etree.SubElement(refsect, "variablelist")
            continue
        param_list = refsect.xpath("variablelist")[0].getchildren()
        for param in param_list:
            name = param.xpath("term/parameter")[0].text
            if name not in seen:
                seen.add(name)
                params.append(param)
    return params

def annotate(entry):
    """
    Numbers all overloaded entries.
    """
    sys.stderr.write("entry: %s\n" % etree.tostring(entry))
    num_overloads = len(entry.xpath("refsynopsisdiv/programlisting"))
    synopsis = entry.xpath("refsynopsisdiv/programlisting")
    description = entry.xpath("refsect1[2]/para")
    for i in range(num_overloads):
        synopsis[i].text = str(i+1) + ")" + " " + synopsis[i].text.strip()
        description[i].text = str(i+1) + ")" + " " + description[i].text.strip()

def merge(functions):
    """
    Merge matching refentries into one and delete them from their parents.
    """
    merged = functions[0]

    sys.stderr.write("processing item %s\n" % merged.xpath("refnamediv/refname")[0].text)
    
    # merge params
    new_params = get_params(functions)
    param_list = merged.xpath("refsect1[1]/variablelist")[0]
    for param in param_list:
        param.getparent().remove(param)
    for param in new_params:
        param_list.append(param)

    # merge synopsis and descriptions
    description = merged.xpath("refsect1[2]")[0]
    synopsis = merged.xpath("refsynopsisdiv")[0]
    for overload in functions[1:]:
        synopsis.append(overload.xpath("refsynopsisdiv/programlisting")[0])
        description.append(overload.xpath("refsect1[2]/para")[0])
        overload.getparent().remove(overload)

    annotate(merged)

def merge_overloads(functions_list):
    for functions in functions_list.values():
        merge(functions)

def usage():
    print "Usage: ./overload.py <xml>"

def main():
    if len(sys.argv) != 2:
        usage()
        sys.exit()
    parser = etree.XMLParser(remove_comments=False)
    tree = etree.parse(sys.argv[1], parser=parser)
    root = tree.getroot()
    refentries = [r for r in root.iter("refentry")]
    functions = collect_overloads(refentries)
    merge_overloads(functions)
    print etree.tostring(root, encoding='UTF-8', xml_declaration=True,
            doctype=tree.docinfo.doctype)

if __name__ == '__main__':
    main()
