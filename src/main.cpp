#include "SKSE/Trampoline.h"
#include <xbyak\xbyak.h>

extern "C" DLLEXPORT bool SKSEAPI SKSEPlugin_Query(const SKSE::QueryInterface* a_skse, SKSE::PluginInfo* a_info)
{
#ifndef NDEBUG
	auto sink = std::make_shared<spdlog::sinks::msvc_sink_mt>();
#else
	auto path = logger::log_directory();
	if (!path) {
		return false;
	}

	*path /= "ContinueSound.log"sv;
	auto sink = std::make_shared<spdlog::sinks::basic_file_sink_mt>(path->string(), true);
#endif

	auto log = std::make_shared<spdlog::logger>("global log"s, std::move(sink));

#ifndef NDEBUG
	log->set_level(spdlog::level::trace);
#else
	log->set_level(spdlog::level::info);
	log->flush_on(spdlog::level::warn);
#endif

	spdlog::set_default_logger(std::move(log));
	spdlog::set_pattern("%g(%#): [%^%l%$] %v"s);

	logger::info("ContinueSound v1.0.0");

	a_info->infoVersion = SKSE::PluginInfo::kVersion;
	a_info->name = "ContinueSound";
	a_info->version = 1;

	if (a_skse->IsEditor()) {
		logger::critical("Loaded in editor, marking as incompatible"sv);
		return false;
	}

	const auto ver = a_skse->RuntimeVersion();
	if (ver < SKSE::RUNTIME_1_5_39) {
		logger::critical(FMT_STRING("Unsupported runtime version {}"), ver.string());
		return false;
	}

	return true;
}

template <size_t BRANCH_TYPE, uint64_t ID, size_t offset = 0>
void add_trampoline(Xbyak::CodeGenerator* xbyakCode)
{
	constexpr REL::ID funcOffset = REL::ID(ID);
	auto funcAddr = funcOffset.address();
	auto size = xbyakCode->getSize();
	auto& trampoline = SKSE::GetTrampoline();
	auto result = trampoline.allocate(size);
	std::memcpy(result, xbyakCode->getCode(), size);
	trampoline.write_branch<BRANCH_TYPE>(funcAddr + offset, (std::uintptr_t)result);
}

uint64_t* data_ptr;
char* str;

void apply_oncontunue()
{
	logger::info("apply_oncontunue");
	const int ID = 51245, BRANCH_TYPE = 5;
	constexpr REL::ID funcOffset = REL::ID(ID);
	auto funcAddr = funcOffset.address();

	constexpr REL::ID subOffset(52014);
	auto subAddr = subOffset.address();

	struct Code : Xbyak::CodeGenerator
	{
		Code(uintptr_t ret_addr, uintptr_t sub_addr)
		{
			mov(rax, (uintptr_t)&data_ptr);
			mov(rax, ptr[rax]);
			mov(dword[rax], 0xfe314159);
			mov(rax, ret_addr);
			push(rax);
			mov(rax, sub_addr);
			jmp(rax);
		}
	} xbyakCode{ funcAddr + 0x16 + BRANCH_TYPE, subAddr };
	add_trampoline<BRANCH_TYPE, ID, 0x16>(static_cast<Xbyak::CodeGenerator*>(&xbyakCode));
}

void apply_playsound()
{
	logger::info("apply_playsound");
	const int ID = 52051, BRANCH_TYPE = 5;
	constexpr REL::ID funcOffset = REL::ID(ID);

	constexpr REL::ID subOffset(52054);
	auto subAddr = subOffset.address();

	logger::info("apply_playsound1");

	struct Code : Xbyak::CodeGenerator
	{
		Code(uintptr_t sub_addr)
		{
			Xbyak::Label jump;

			mov(rax, (uintptr_t)&data_ptr);
			mov(rax, ptr[rax]);
			cmp(dword[rax], 0xfe314159);
			jne(jump);
			mov(dword[rax], 0x0);
			mov(rax, (uintptr_t)&str);
			mov(rcx, ptr[rax]);

			L(jump);
			mov(rax, sub_addr);
			jmp(rax);
		}
	} xbyakCode{ subAddr };
	logger::info("apply_playsound, add_trampoline");
	add_trampoline<BRANCH_TYPE, ID, 0x1B>(static_cast<Xbyak::CodeGenerator*>(&xbyakCode));
	logger::info("apply_playsound, add_trampoline OK");
}

void apply_hooks()
{
	logger::info("apply_hooks");
	SKSE::AllocTrampoline(1 << 10);

	data_ptr = new uint64_t(0);
	const char s[15] = "UIStartNewGame";
	constexpr size_t s_len = sizeof(s);
	str = new char[s_len];
	memcpy(str, s, s_len);

	apply_playsound();
	apply_oncontunue();
}

extern "C" DLLEXPORT bool SKSEAPI SKSEPlugin_Load(const SKSE::LoadInterface* a_skse)
{
	logger::info("ContinueSound loaded");

	SKSE::Init(a_skse);

	apply_hooks();

	return true;
}
