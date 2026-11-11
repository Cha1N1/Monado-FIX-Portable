"""Source-level regression checks for Vulkan capability selection."""
from pathlib import Path
import unittest

ROOT = Path(__file__).resolve().parents[2]
DEVICE = (ROOT / "runtime/nvn/vulkan/device.c").read_text(encoding="utf-8")


class VulkanSourceContracts(unittest.TestCase):
    def test_validation_fallback_removes_debug_extension_by_name(self):
        self.assertIn("strcmp(ext[i], VK_EXT_DEBUG_UTILS_EXTENSION_NAME) != 0", DEVICE)
        self.assertNotIn("ci.enabledExtensionCount--;", DEVICE)

    def test_optional_feature_bits_are_copied_from_physical_device_support(self):
        self.assertIn(".shaderInt8 = h12.shaderInt8", DEVICE)
        self.assertIn(".storageBuffer8BitAccess = h12.storageBuffer8BitAccess", DEVICE)
        self.assertIn(".scalarBlockLayout = h12.scalarBlockLayout", DEVICE)
        self.assertIn(".storageBuffer16BitAccess = h11.storageBuffer16BitAccess", DEVICE)
        self.assertNotIn(".shaderInt8 = VK_TRUE", DEVICE)
        self.assertNotIn(".storageBuffer16BitAccess = VK_TRUE", DEVICE)

    def test_backend_required_features_fail_with_explicit_diagnostic(self):
        self.assertIn("!h13.dynamicRendering || !h13.synchronization2", DEVICE)
        self.assertIn("!h12.timelineSemaphore", DEVICE)

    def test_automatic_selection_filters_devices_by_queue_capabilities(self):
        probe = (ROOT / "runtime/nvn/vulkan/gpu_probe.h").read_text(encoding="utf-8")
        self.assertIn("xcde_gpu_enumerate", DEVICE)
        self.assertIn("VK_QUEUE_GRAPHICS_BIT | VK_QUEUE_COMPUTE_BIT", probe)
        self.assertIn("vkGetPhysicalDeviceSurfaceSupportKHR", probe)
        self.assertIn("LARECOMP_GPU", DEVICE)

    def test_pipeline_cache_input_is_bounded_and_allocation_checked(self):
        self.assertIn("end <= 128L * 1024L * 1024L", DEVICE)
        self.assertIn("if (!data || fread(data, 1, sz, fp) != sz)", DEVICE)
        self.assertIn("pipeline cache ignored: invalid size", DEVICE)

    def test_pipeline_cache_save_checks_size_and_allocation(self):
        self.assertIn("sz > 128u * 1024u * 1024u", DEVICE)
        self.assertIn("pipeline cache not saved: out of memory", DEVICE)

    def test_vulkan_memory_budget_extension_is_opt_in_setting(self):
        self.assertIn("g_settings.gpu_memory_budget && has_ext", DEVICE)
        menu = (ROOT / "runtime/ui/menu_vk.cpp").read_text(encoding="utf-8")
        self.assertIn("vmaGetHeapBudgets(g_vk.vma, budgets)", menu)
        self.assertIn("VK_PHYSICAL_DEVICE_TYPE_INTEGRATED_GPU", menu)


    def test_optional_feature_log_is_after_feature_struct_initialization(self):
        self.assertLess(DEVICE.index("VkPhysicalDeviceVulkan11Features f11 ="),
                        DEVICE.index("[vk] optional features:"))

    def test_integrated_gpu_memory_is_not_assumed_to_be_dedicated_vram(self):
        self.assertIn("VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT", DEVICE)
        self.assertIn("shared_bytes", DEVICE)
        self.assertIn("DEVICE_LOCAL alone does not mean dedicated VRAM", DEVICE)

    def test_extension_feature_bits_are_queried_before_enable(self):
        self.assertIn("cbc_have.customBorderColors", DEVICE)
        self.assertIn("div_have.vertexAttributeInstanceRateDivisor", DEVICE)
        self.assertIn("ffhave.deviceFault", DEVICE)

    def test_manual_gpu_policy_is_persisted_and_used(self):
        settings = (ROOT / "runtime/host/settings_ini.c").read_text(encoding="utf-8")
        header = (ROOT / "runtime/settings.h").read_text(encoding="utf-8")
        self.assertIn('"gpu_index"', settings)
        self.assertIn('"gpu_profile"', settings)
        self.assertIn('"gpu_gpl"', settings)
        self.assertIn('"gpu_memory_budget"', settings)
        self.assertIn('"amd_fix"', settings)
        self.assertIn("AMD_FIX_AUTO", header)
        self.assertIn("g_settings.gpu_index", DEVICE)
        self.assertIn("GPU_PROFILE_COMPATIBILITY", header)

    def test_gpu_menu_exposes_restart_scoped_controls_and_diagnostics(self):
        menu = (ROOT / "runtime/ui/menu_vk.cpp").read_text(encoding="utf-8")
        self.assertIn('BeginTabItem(T("GPU / Vulkan"))', menu)
        self.assertIn("g_vk.props.deviceName", menu)
        self.assertIn("g_settings.gpu_memory_budget", menu)
        self.assertIn("g_vk.driver_props.driverName", menu)
        self.assertIn("These changes require restarting the game", menu)
        temporal = (ROOT / "runtime/nvn/vulkan/temporal_fg.inc").read_text(encoding="utf-8")
        self.assertIn("g_settings.gpu_profile != GPU_PROFILE_COMPATIBILITY", temporal)
        self.assertIn("g_settings.gpu_profile != GPU_PROFILE_COMPATIBILITY", DEVICE)


if __name__ == "__main__":
    unittest.main()
