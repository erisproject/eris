import os
import re
import ycm_core

flags = [
        '-Wall',
        '-Wextra',
        '-std=c++11',
        '-I', '.',
        '-I', '/usr/include/eigen3',
        '-DERIS_TESTS' # Needed for test scripts to compile
        ]

def DirectoryOfThisScript():
    return os.path.dirname( os.path.abspath( __file__ ) )

def MakeRelativePathsInFlagsAbsolute( flags, working_directory ):
    if not working_directory:
        return list( flags )
    new_flags = []
    make_next_absolute = False
    path_flags = [ '-isystem', '-I', '-iquote', '--sysroot=' ]
    for flag in flags:
        new_flag = flag

        if make_next_absolute:
            make_next_absolute = False
            if not flag.startswith( '/' ):
                new_flag = os.path.join( working_directory, flag )

        for path_flag in path_flags:
            if flag == path_flag:
                make_next_absolute = True
                break

            if flag.startswith( path_flag ):
                path = flag[ len( path_flag ): ]
                new_flag = path_flag + os.path.join( working_directory, path )
                break

        if new_flag:
            new_flags.append( new_flag )
    return new_flags

# Dirty hack until ycm #2330 is solved
def LoadSystemIncludes():
    regex = re.compile(ur'(?:\#include \<...\> search starts here\:)(?P<list>.*?)(?:End of search list)', re.DOTALL);
    process = subprocess.Popen(['clang', '-v', '-E', '-x', 'c++', '-'], stdin=subprocess.PIPE, stdout=subprocess.PIPE, stderr=subprocess.PIPE);
    process_out, process_err = process.communicate('');
    output = process_out + process_err;
    includes = [];
    for p in re.search(regex, output).group('list').split('\n'):
        p = p.strip();
        if len(p) > 0 and p.find('(framework directory)') < 0:
            includes.append('-isystem');
            includes.append(p);
    return includes;

def FlagsForFile( filename, **kwargs ):
    final_flags = MakeRelativePathsInFlagsAbsolute( flags, DirectoryOfThisScript() ) + LoadSystemIncludes()
    return {
            'flags': final_flags,
            'do_cache': True
            }
