add_rules('mode.release', 'mode.debug', 'mode.releasedbg')

set_languages('cxx17', 'c11')
set_warnings('all')
set_exceptions('cxx')

-- 使用 xmake 包管理
add_requires("sqlite3")

if is_mode('releasedbg') then
    set_symbols('debug')
    set_optimize('fast')
end

target('words_plugin')
    set_kind('shared')
    add_rules('qt.shared')

    add_files('src/*.cpp')
    add_files('src/*.h')

    add_frameworks(
        'QtCore',
        'QtQuick',
        'QtQml',
        'QtGui'
    )

    -- 引入 sqlite3 包
    add_packages("sqlite3")
