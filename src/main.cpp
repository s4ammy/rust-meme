#include "../build/debug/rs/src/memflow/memflow-ffi/memflow.hpp"

template<typename t>
CSliceRef<uint8_t> make_ref(t data)//github copilot
{
    CSliceRef<uint8_t> ref {};
    ref.data = (uint8_t*)&data;
    ref.len = sizeof(data);

    return ref;
}

template<typename t>
CSliceMut<uint8_t> make_mut(t data)//github copilot
{
    CSliceMut<uint8_t> mut {};
    mut.data = (uint8_t*)&data;
    mut.len = sizeof(data);

    return mut;
}

template<typename t>
t read_mem(OsInstance<> os, uint64_t address)
{
    t data_out;

    CSliceMut<uint8_t> mut = make_mut(data_out);

    os.read_raw_into(address, mut);

    return data_out;
}

//inverse of read_mem
template<typename t>
void write_mem(OsInstance<> os, uint64_t address, t data)//github copilot
{
    CSliceRef<uint8_t> ref = make_ref(data);

    os.write_raw(address, ref);
}

int main(int argc, char *argv[]) 
{
    log_init(LevelFilter::LevelFilter_Error);

    Inventory *inventory = inventory_scan();
    if (!inventory)
    {
		log_error("unable to create inventory");

		return 1;
	}

	const char *conn_name = argc > 1? argv[1]: "qemu";
	const char *conn_arg = argc > 2? argv[2]: "";
	const char *os_name = argc > 3? argv[3]: "win32";
	const char *os_arg = argc > 4? argv[4]: "";

	ConnectorInstance<> connector, *conn = conn_name[0] ? &connector : nullptr;

    if (conn)
    {
        if (inventory_create_connector(inventory, conn_name, conn_arg, &connector))
        {
			log_error("unable to initialize connector");

			inventory_free(inventory);
			return 1;
		}
	}

	OsInstance<> os;
	if (inventory_create_os(inventory, os_name, os_arg, conn, &os))
    {
        log_error("unable to initialize os");

		inventory_free(inventory);
		return 1;
	}

	inventory_free(inventory);

    ProcessInstance<CBox<void>, CArc<void>> proc;
    os.process_by_name("RustClient.exe", &proc);

    bool is_alive = proc.state().tag == ProcessState::Tag::ProcessState_Alive;
    if (!is_alive)
    {
        log_error("unable to find RustClient.exe");

        return 1;
    }

    printf("found RustClient.exe running, path -> %s\n", proc.info()->path);

    ModuleInfo assembly_module, unity_module;
    proc.module_by_name("GameAssembly.dll", &assembly_module);
    proc.module_by_name("UnityPlayer.dll", &unity_module);

    uint64_t assembly_base = assembly_module.base;
    uint64_t assembly_size = assembly_module.size;

    uint64_t unity_base = unity_module.base;
    uint64_t unity_size = unity_module.size;

    //OcclusionCulling_TypeInfo
    uint64_t occlusion_culling_type_info = read_mem<uint64_t>(os, assembly_base + 0x0);
    if (!occlusion_culling_type_info)
	{
        log_error("unable to find occlusion culling type info");

        return 1;
    }

    printf("found occlusion culling type info at 0x%llx\n", occlusion_culling_type_info);

    uint64_t culling_class = read_mem<uint64_t>(os, occlusion_culling_type_info + 0xB8);
	if (!culling_class)
	{
        log_error("unable to find culling class");

        return 1;
    }

    printf("found culling class at 0x%llx\n", culling_class);

	write_mem<int>(os, culling_class + 0x94, 1);

    log_info("successfully patched occlusion culling");

    return 0;
}