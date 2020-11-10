#include "daScript/daScript.h"
#include "daScript/simulate/fs_file_info.h"
#include "daScript/misc/performance_time.h"
#include "daScript/misc/fpe.h"
#include "daScript/misc/sysos.h"

#ifdef _MSC_VER
#include <io.h>
#else
#include <dirent.h>
#endif

using namespace das;

#if !defined(DAS_GLOBAL_NEW) && defined(_MSC_VER) && !defined(_WIN64)

void * operator new(std::size_t n) throw(std::bad_alloc)
{
    return das_aligned_alloc16(n);
}
void operator delete(void * p) throw()
{
    das_aligned_free16(p);
}

#endif

bool g_reportCompilationFailErrors = false;

TextPrinter tout;

bool compilation_fail_test ( const string & fn, bool ) {
    uint64_t timeStamp = ref_time_ticks();
    tout << fn << " ";
    auto fAccess = make_smart<FsFileAccess>();
    ModuleGroup dummyLibGroup;
    if ( auto program = compileDaScript(fn, fAccess, tout, dummyLibGroup) ) {
        if ( program->failed() ) {
            // we allow circular module dependency to fly through
            if ( program->errors.size()==1 ) {
                if ( program->errors[0].cerr==CompilationError::module_not_found ) {
                    if ( fn.find("circular_module_dependency")!=string::npos ) {
                        tout << "ok\n";
                        return true;
                    }
                }
            }
            // regular error processing
            bool failed = false;
            auto errors = program->expectErrors;
            for ( auto err : program->errors ) {
                int count = -- errors[err.cerr];
                if ( g_reportCompilationFailErrors || count<0 ) {
                    tout << reportError(err.at, err.what, err.extra, err.fixme, err.cerr);
                }
                if ( count <0 ) {
                    failed = true;
                }
            }
            bool any_errors = false;
            for ( auto err : errors ) {
                if ( err.second > 0 ) {
                    any_errors = true;
                    break;
                }
            }
            if ( any_errors || failed ) {
                tout << "failed";
                if ( any_errors ) {
                    tout << ", expecting errors";
                    for ( auto terr : errors  ) {
                        if (terr.second > 0 ) {
                            tout << " " << int(terr.first) << ":" << terr.second;
                        }
                    }
                }
                tout << "\n";
                return false;
            }
            int usec = get_time_usec(timeStamp);
            tout << "ok " << ((usec/1000)/1000.0) << "\n";
            return true;
        } else {
            tout << "failed, compiled without errors\n";
            return false;
        }
    } else {
        tout << "failed, not expected to compile\n";
        return false;
    }
}

bool unit_test ( const string & fn, bool useAot ) {
    uint64_t timeStamp = ref_time_ticks();
    tout << fn << " ";
    auto fAccess = make_smart<FsFileAccess>();
    ModuleGroup dummyLibGroup;
    CodeOfPolicies policies;
    policies.fail_on_no_aot = true;
    // policies.intern_strings = true;
    // policies.intern_const_strings = true;
    // policies.no_unsafe = true;
    if ( auto program = compileDaScript(fn, fAccess, tout, dummyLibGroup, false, policies) ) {
        if ( program->failed() ) {
            tout << "failed to compile\n";
            for ( auto & err : program->errors ) {
                tout << reportError(err.at, err.what, err.extra, err.fixme, err.cerr );
            }
            return false;
        } else {
            if (program->unsafe) tout << "[unsafe] ";
            Context ctx(program->getContextStackSize());
            if ( !program->simulate(ctx, tout) ) {
                tout << "failed to simulate\n";
                for ( auto & err : program->errors ) {
                    tout << reportError(err.at, err.what, err.extra, err.fixme, err.cerr );
                }
                return false;
            }
            if ( useAot ) {
                // now, what we get to do is to link AOT
                AotLibrary aotLib;
                AotListBase::registerAot(aotLib);
                program->linkCppAot(ctx, aotLib, tout);
                if ( program->failed() ) {
                    tout << "failed to link AOT\n";
                    for ( auto & err : program->errors ) {
                        tout << reportError(err.at, err.what, err.extra, err.fixme, err.cerr );
                    }
                    return false;
                }
            }
            if ( auto fnTest = ctx.findFunction("test") ) {
                if ( !verifyCall<bool>(fnTest->debugInfo, dummyLibGroup) ) {
                    tout << "function 'test', call arguments do not match\n";
                    return false;
                }
                ctx.restart();
                ctx.runInitScript();    // this is here for testing purposes only
                bool result = cast<bool>::to(ctx.eval(fnTest, nullptr));
                if ( auto ex = ctx.getException() ) {
                    tout << "exception: " << ex << "\n";
                    return false;
                }
                if ( !result ) {
                    tout << "failed\n";
                    return false;
                }
                int usec = get_time_usec(timeStamp);
                tout << (useAot ? "ok AOT " : "ok ") << ((usec/1000)/1000.0) << "\n";
                return true;
            } else {
                tout << "function 'test' not found\n";
                return false;
            }
        }
    } else {
        return false;
    }
}

bool exception_test ( const string & fn, bool useAot ) {
    tout << fn << " ";
    auto fAccess = make_smart<FsFileAccess>();
    ModuleGroup dummyLibGroup;
    if ( auto program = compileDaScript(fn, fAccess, tout, dummyLibGroup) ) {
        if ( program->failed() ) {
            tout << "failed to compile\n";
            for ( auto & err : program->errors ) {
                tout << reportError(err.at, err.what, err.extra, err.fixme, err.cerr );
            }
            return false;
        } else {
            Context ctx(program->getContextStackSize());
            if ( !program->simulate(ctx, tout) ) {
                tout << "failed to simulate\n";
                for ( auto & err : program->errors ) {
                    tout << reportError(err.at, err.what, err.extra, err.fixme, err.cerr );
                }
                return false;
            }
            if ( useAot ) {
                // now, what we get to do is to link AOT
                AotLibrary aotLib;
                AotListBase::registerAot(aotLib);
                program->linkCppAot(ctx, aotLib, tout);
            }
            if ( auto fnTest = ctx.findFunction("test") ) {
                if ( !verifyCall<bool>(fnTest->debugInfo, dummyLibGroup) ) {
                    tout << "function 'test', call arguments do not match\n";
                    return false;
                }
                ctx.restart();
                ctx.evalWithCatch(fnTest, nullptr);
                if ( auto ex = ctx.getException() ) {
                    tout << "with exception " << ex << ", " << (useAot ? "ok AOT\n" : "ok\n");
                    return true;
                }
                tout << "failed, finished without exception\n";
                return false;
            } else {
                tout << "function 'test' not found\n";
                return false;
            }
        }
    } else {
        return false;
    }
}

bool run_tests( const string & path, bool (*test_fn)(const string &, bool useAot), bool useAot ) {
#ifdef _MSC_VER
    bool ok = true;
    _finddata_t c_file;
    intptr_t hFile;
    string findPath = path + "/*.das";
    if ((hFile = _findfirst(findPath.c_str(), &c_file)) != -1L) {
        do {
            const char * atDas = strstr(c_file.name,".das");
            if ( atDas && strcmp(atDas,".das")==0 && c_file.name[0]!='_' ) {
                ok = test_fn(path + "/" + c_file.name, useAot) && ok;
            }
        } while (_findnext(hFile, &c_file) == 0);
    }
    _findclose(hFile);
    return ok;
#else
    bool ok = true;
    DIR *dir;
    struct dirent *ent;
    if ((dir = opendir (path.c_str())) != NULL) {
        while ((ent = readdir (dir)) != NULL) {
            const char * atDas = strstr(ent->d_name,".das");
            if ( atDas && strcmp(atDas,".das")==0 && ent->d_name[0]!='_' ) {
                ok = test_fn(path + "/" + ent->d_name, useAot) && ok;
            }
        }
        closedir (dir);
    }
    return ok;
#endif
}

bool run_unit_tests( const string & path, bool check_aot = false ) {
    if ( check_aot ) {
        return run_tests(path, unit_test, false) && run_tests(path, unit_test, true);
    } else {
        return run_tests(path, unit_test, false);
    }
}

bool run_compilation_fail_tests( const string & path ) {
    return run_tests(path, compilation_fail_test, false);
}

bool run_exception_tests( const string & path ) {
    return run_tests(path, exception_test, false) && run_tests(path, exception_test, true);
}


bool run_module_test ( const string & path, const string & main, bool usePak ) {
    tout << "testing MODULE at " << path << " ";
    auto fAccess = usePak ?
            make_smart<FsFileAccess>( path + "/project.das_project", make_smart<FsFileAccess>()) :
            make_smart<FsFileAccess>();
    ModuleGroup dummyLibGroup;
    if ( auto program = compileDaScript(path + "/" + main, fAccess, tout, dummyLibGroup) ) {
        if ( program->failed() ) {
            tout << "failed to compile\n";
            for ( auto & err : program->errors ) {
                tout << reportError(err.at, err.what, err.extra, err.fixme, err.cerr );
            }
            return false;
        } else {
            Context ctx(program->getContextStackSize());
            if ( !program->simulate(ctx, tout) ) {
                tout << "failed to simulate\n";
                for ( auto & err : program->errors ) {
                    tout << reportError(err.at, err.what, err.extra, err.fixme, err.cerr );
                }
                return false;
            }
            if ( auto fnTest = ctx.findFunction("test") ) {
                if ( !verifyCall<bool>(fnTest->debugInfo, dummyLibGroup) ) {
                    tout << "function 'test', call arguments do not match\n";
                    return false;
                }
                ctx.restart();
                ctx.runInitScript();    // this is here for testing purposes only
                bool result = cast<bool>::to(ctx.eval(fnTest, nullptr));
                if ( auto ex = ctx.getException() ) {
                    tout << "exception: " << ex << "\n";
                    return false;
                }
                if ( !result ) {
                    tout << "failed\n";
                    return false;
                }
                tout << "ok\n";
                return true;
            } else {
                tout << "function 'test' not found\n";
                return false;
            }
        }
    } else {
        return false;
    }
}

extern int das_yydebug;

int main( int argc, char * argv[] ) {
    if ( argc>2 ) {
        tout << "daScriptTest [pathToDasRoot]\n";
        return -1;
    }  else if ( argc==2 ) {
        setDasRoot(argv[1]);
    }
    setCommandLineArguments(argc,argv);
    // ptr_ref_count::ref_count_track = 0x1242c;
    // das_track_string_breakpoint(189);
    // das_track_breakpoint(8);
    // _CrtSetDbgFlag(_CRTDBG_ALLOC_MEM_DF | _CRTDBG_LEAK_CHECK_DF);
    // _CrtSetBreakAlloc(6836533);
    _mm_setcsr((_mm_getcsr()&~_MM_ROUND_MASK) | _MM_FLUSH_ZERO_MASK | _MM_ROUND_NEAREST | 0x40);//0x40
    FPE_ENABLE_ALL;
    // register modules
    NEED_MODULE(Module_BuiltIn);
    NEED_MODULE(Module_Math);
    NEED_MODULE(Module_Strings);
    NEED_MODULE(Module_UnitTest);
    NEED_MODULE(Module_Rtti);
    NEED_MODULE(Module_Ast);
    NEED_MODULE(Module_Debugger);
    NEED_MODULE(Module_FIO);
    NEED_MODULE(Module_Random);
    NEED_MODULE(Module_Network);
    NEED_MODULE(Module_UriParser);
#if 0 // Debug this one test
    compilation_fail_test(getDasRoot() + "/examples/test/compilation_fail_tests/smart_ptr.das",true);
    Module::Shutdown();
    getchar();
    return 0;
#endif
#if 0 // Debug this one test
// generators
    // #define TEST_NAME   "/dasgen/gen_bind.das"
    // #define TEST_NAME   "/doc/reflections/das2rst.das"
// examples
    // #define TEST_NAME   "/examples/test/dict_pg.das"
    #define TEST_NAME   "/examples/test/hello_world.das"
    // #define TEST_NAME   "/examples/test/base64.das"
    // #define TEST_NAME   "/examples/test/regex_lite.das"
    // #define TEST_NAME   "/examples/test/hello_world.das"
    // #define TEST_NAME   "/examples/test/json_example.das"
    // #define TEST_NAME   "/examples/test/ast_print.das"
    // #define TEST_NAME   "/examples/test/unit_tests/hint_macros_example.das"
    // #define TEST_NAME   "/examples/test/unit_tests/aonce.das"
    unit_test(getDasRoot() +  TEST_NAME,false);
    // unit_test(getDasRoot() +  TEST_NAME,true);
    Module::Shutdown();
    // dumpTrackingLeaks();
    getchar();
    return 0;
#endif
#if 0 // Module test
    run_module_test(getDasRoot() +  "/examples/test/module", "main_inc.das", true);
    Module::Shutdown();
    getchar();
    return 0;
#endif
    uint64_t timeStamp = ref_time_ticks();
    bool ok = true;
    ok = run_compilation_fail_tests(getDasRoot() + "/examples/test/compilation_fail_tests") && ok;
    ok = run_unit_tests(getDasRoot() +  "/examples/test/unit_tests", true) && ok;
    ok = run_unit_tests(getDasRoot() +  "/examples/test/optimizations") && ok;
    ok = run_exception_tests(getDasRoot() +  "/examples/test/runtime_errors") && ok;
    ok = run_module_test(getDasRoot() +  "/examples/test/module", "main.das", true) && ok;
    ok = run_module_test(getDasRoot() +  "/examples/test/module", "main_inc.das", true)  && ok;
    ok = run_module_test(getDasRoot() +  "/examples/test/module", "main_default.das", false) && ok;
    ok = run_module_test(getDasRoot() +  "/examples/test/module/alias", "main.das", true) && ok;
    ok = run_module_test(getDasRoot() +  "/examples/test/module/cdp", "main.das", true) && ok;
    int usec = get_time_usec(timeStamp);
    tout << "TESTS " << (ok ? "PASSED " : "FAILED!!! ") << ((usec/1000)/1000.0) << "\n";
    // shutdown
    Module::Shutdown();
    return ok ? 0 : -1;
}
