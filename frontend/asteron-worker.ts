/**
 * =============================================================================
 * ASTERON WEB WORKER
 * =============================================================================
 * 
 * Web Worker que executa o compilador Asteron (Wasm) e retorna
 * dados formatados para o React Flow.
 * 
 * =============================================================================
 */

// Importa módulo Wasm (gerado pelo Emscripten)
import AsteronWasm from '../public/asteron.js';

// Tipos
export interface GraphNode {
  id: string;
  data: {
    label: string;
    executionCount?: number;
    avgTime?: number;
    isHot?: boolean;
  };
  position?: { x: number; y: number };
  style?: {
    background?: string;
    color?: string;
  };
}

export interface GraphEdge {
  id: string;
  source: string;
  target: string;
  label?: string;
}

export interface GraphData {
  nodes: GraphNode[];
  edges: GraphEdge[];
}

export interface HotPath {
  name: string;
  executionCount: number;
  avgTime: number;
}

// Estado do Worker
let wasmModule: any = null;
let isInitialized = false;

// Inicializa módulo Wasm
async function initWasm(): Promise<void> {
  if (isInitialized) return;

  try {
    wasmModule = await AsteronWasm({
      locateFile: (path: string) => {
        if (path.endsWith('.wasm')) {
          return '/asteron.wasm';
        }
        return path;
      }
    });
    isInitialized = true;
    console.log('[Asteron Worker] Wasm inicializado');
  } catch (error) {
    console.error('[Asteron Worker] Erro ao inicializar Wasm:', error);
    throw error;
  }
}

// Compila código e retorna AST
export async function compileToAST(code: string): Promise<any> {
  await initWasm();

  try {
    const compileFn = wasmModule.cwrap('compile_to_ast', 'string', ['string']);
    const jsonStr = compileFn(code);
    return JSON.parse(jsonStr);
  } catch (error) {
    console.error('[Asteron Worker] Erro ao compilar:', error);
    throw error;
  }
}

// Compila código e retorna grafo (formato React Flow)
export async function compileToGraph(code: string): Promise<GraphData> {
  await initWasm();

  try {
    const compileFn = wasmModule.cwrap('compile_to_graph', 'string', ['string']);
    const jsonStr = compileFn(code);
    const data = JSON.parse(jsonStr);

    // Formata para React Flow
    return {
      nodes: data.nodes || [],
      edges: data.edges || []
    };
  } catch (error) {
    console.error('[Asteron Worker] Erro ao gerar grafo:', error);
    throw error;
  }
}

// Obtém grafo atual
export async function getGraph(): Promise<GraphData> {
  await initWasm();

  try {
    const getGraphFn = wasmModule.cwrap('get_graph_json', 'string', []);
    const jsonStr = getGraphFn();
    const data = JSON.parse(jsonStr);

    return {
      nodes: data.nodes || [],
      edges: data.edges || []
    };
  } catch (error) {
    console.error('[Asteron Worker] Erro ao obter grafo:', error);
    throw error;
  }
}

// Obtém hot paths
export async function getHotPaths(): Promise<HotPath[]> {
  await initWasm();

  try {
    const getHotPathsFn = wasmModule.cwrap('get_hot_paths', 'string', []);
    const jsonStr = getHotPathsFn();
    const data = JSON.parse(jsonStr);

    return data.hotPaths || [];
  } catch (error) {
    console.error('[Asteron Worker] Erro ao obter hot paths:', error);
    throw error;
  }
}

// Obtém métricas de um nó
export async function getNodeMetrics(nodeName: string): Promise<any> {
  await initWasm();

  try {
    const getMetricsFn = wasmModule.cwrap('get_node_metrics', 'string', ['string']);
    const jsonStr = getMetricsFn(nodeName);
    return JSON.parse(jsonStr);
  } catch (error) {
    console.error('[Asteron Worker] Erro ao obter métricas:', error);
    throw error;
  }
}

// Handler do Web Worker
self.onmessage = async (event: MessageEvent) => {
  const { type, payload } = event.data;

  try {
    switch (type) {
      case 'COMPILE_TO_AST':
        const ast = await compileToAST(payload.code);
        self.postMessage({ type: 'AST_RESULT', payload: ast });
        break;

      case 'COMPILE_TO_GRAPH':
        const graph = await compileToGraph(payload.code);
        self.postMessage({ type: 'GRAPH_RESULT', payload: graph });
        break;

      case 'GET_GRAPH':
        const currentGraph = await getGraph();
        self.postMessage({ type: 'GRAPH_RESULT', payload: currentGraph });
        break;

      case 'GET_HOT_PATHS':
        const hotPaths = await getHotPaths();
        self.postMessage({ type: 'HOT_PATHS_RESULT', payload: hotPaths });
        break;

      case 'GET_NODE_METRICS':
        const metrics = await getNodeMetrics(payload.nodeName);
        self.postMessage({ type: 'METRICS_RESULT', payload: metrics });
        break;

      default:
        self.postMessage({ type: 'ERROR', payload: `Unknown type: ${type}` });
    }
  } catch (error) {
    self.postMessage({ 
      type: 'ERROR', 
      payload: error instanceof Error ? error.message : String(error) 
    });
  }
};

