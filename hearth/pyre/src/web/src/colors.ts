import type { Target } from './types';

// Same hues in light and dark (the series stay recognisable when the theme flips).
export const TARGET_COLOR: Record<Target, string> = { cpu: '#64748b', cuda: '#16a34a', vulkan: '#d97706' };
export const TARGET_LABEL: Record<Target, string> = { cpu: 'CPU', cuda: 'CUDA', vulkan: 'Vulkan' };
