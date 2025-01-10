'''
Custom installation for ARBFN in LAMMPS.
Jordan Dehmel, 2025
'''

import sys
import os
import os.path as path
import shutil
import subprocess as sp


def main() -> int:
    '''
    Main function
    '''

    print('This is a custom installation script for the ARBFN LAMMPS package.')

    if os.geteuid() != 0:
        print('Please re-run as sudo.')
        return 1

    lammps_path: str = input(
        'Please enter the filepath to the `lammps` source code repository: ')
    lammps_path = path.abspath(lammps_path)

    copy_source: str = path.abspath('ARBFN')
    copy_target: str = path.join(lammps_path, 'src')

    assert path.exists(lammps_path) and path.isdir(lammps_path)
    assert path.exists(copy_target)
    assert path.isdir(copy_target)

    assert path.exists(copy_source)
    assert path.isdir(copy_source)

    print('Copying package source code...')

    if path.exists(path.join(copy_target, 'ARBFN')):
        shutil.rmtree(path.join(copy_target, 'ARBFN'))

    shutil.copytree(copy_source, path.join(copy_target, 'ARBFN'))

    # Ensure that {lammps_path}/cmake/CMakeLists.txt contains ARBFN
    cmakelists_path: str = path.join(lammps_path, 'cmake', 'CMakeLists.txt')
    assert path.exists(cmakelists_path)
    assert path.isfile(cmakelists_path)

    # set(STANDARD_PACKAGES ... )
    prefix: str = ''
    suffix: str = ''
    found = False
    on_prefix = True

    with open(cmakelists_path, 'r', encoding='utf8') as f:
        lines = f.readlines()
        for line in lines:
            if on_prefix:
                prefix += line
            else:
                suffix += line

            line = line.strip()
            if line == 'ARBFN':
                found = True
                break
            elif line == 'set(STANDARD_PACKAGES':
                on_prefix = False

    if found:
        print('ARBFN was already registered in cmake: Continuing.')
    else:
        print('Not found...')

        with open(cmakelists_path, 'w', encoding='utf8') as f:
            f.write(prefix)
            f.write('  ARBFN\n')
            f.write(suffix)

    if input('Build using cmake? [y/n]: ').lower() == 'y':

        # Create cmake build dir
        build_dir: str = path.join(lammps_path, 'build')

        if not path.exists(build_dir):
            os.mkdir(build_dir)

        # Log CWD and CD
        old_path: str = os.getcwd()
        os.chdir(build_dir)

        # Call cmake
        sp.run(['cmake', '-C', '../cmake/presets/all_on.cmake',
                '-C', '../cmake/presets/nolib.cmake', '-D',
                'PKG_ARBFN=ON', '-D', 'BUILD_MPI=yes',
                '../cmake'], check=True)

        # Compile executable
        sp.run(['make'], check=True)

        # Install executable
        exe_source: str = path.join(build_dir, 'lmp')
        exe_target: str = '/usr/bin/lmp'

        shutil.copyfile(exe_source, exe_target)

        # CD back
        os.chdir(old_path)

    print('Done.')

    return 0


if __name__ == '__main__':
    sys.exit(main())
