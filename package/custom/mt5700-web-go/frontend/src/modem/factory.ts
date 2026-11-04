export const isFactoryODU = window.location.pathname.startsWith('/modem/external/');

export async function factoryRequest(payload?: Record<string, unknown>): Promise<any> {
  const response = await fetch('/modem/external/native', payload ? {
    method: 'POST', headers: { 'Content-Type': 'application/json' }, body: JSON.stringify(payload),
  } : { cache: 'no-store' });
  const result = await response.json();
  if (!result.ok) throw new Error(result.error || '原厂接口返回失败');
  return result.state;
}
