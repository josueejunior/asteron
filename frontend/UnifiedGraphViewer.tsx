/**
 * =============================================================================
 * UNIFIED GRAPH VIEWER (React Component)
 * =============================================================================
 * 
 * Componente React que visualiza o grafo unificado do Asteron em tempo real.
 * 
 * CARACTERÍSTICAS:
 * - Cores dinâmicas baseadas em "calor" (hot paths)
 * - Atualização em tempo real conforme código é compilado
 * - Interatividade (hover, click, zoom)
 * 
 * =============================================================================
 */

import React, { useEffect, useState, useCallback } from 'react';
import ReactFlow, {
  Node,
  Edge,
  Background,
  Controls,
  MiniMap,
  useNodesState,
  useEdgesState,
  Connection,
  addEdge,
} from 'reactflow';
import 'reactflow/dist/style.css';

interface GraphViewerProps {
  code: string;
  onNodeClick?: (nodeId: string) => void;
}

export const UnifiedGraphViewer: React.FC<GraphViewerProps> = ({ 
  code, 
  onNodeClick 
}) => {
  const [nodes, setNodes, onNodesChange] = useNodesState([]);
  const [edges, setEdges, onEdgesChange] = useEdgesState([]);
  const [isLoading, setIsLoading] = useState(false);
  const [error, setError] = useState<string | null>(null);

  // Worker para compilação
  const workerRef = React.useRef<Worker | null>(null);

  useEffect(() => {
    // Cria worker
    workerRef.current = new Worker(
      new URL('./asteron-worker.ts', import.meta.url),
      { type: 'module' }
    );

    // Handler de mensagens do worker
    workerRef.current.onmessage = (event) => {
      const { type, payload } = event.data;

      switch (type) {
        case 'GRAPH_RESULT':
          updateGraph(payload);
          setIsLoading(false);
          setError(null);
          break;

        case 'ERROR':
          setError(payload);
          setIsLoading(false);
          break;

        default:
          break;
      }
    };

    return () => {
      workerRef.current?.terminate();
    };
  }, []);

  // Atualiza grafo quando código muda
  useEffect(() => {
    if (!code || !workerRef.current) return;

    setIsLoading(true);
    setError(null);

    // Envia código para worker compilar
    workerRef.current.postMessage({
      type: 'COMPILE_TO_GRAPH',
      payload: { code }
    });
  }, [code]);

  // Atualiza nós e arestas do grafo
  const updateGraph = useCallback((data: { nodes: any[]; edges: any[] }) => {
    // Converte para formato React Flow
    const flowNodes: Node[] = data.nodes.map((node, index) => ({
      id: node.id,
      type: 'default',
      data: {
        label: (
          <div>
            <div style={{ fontWeight: 'bold' }}>{node.data.label}</div>
            {node.data.executionCount !== undefined && (
              <div style={{ fontSize: '0.8em', color: '#666' }}>
                Execuções: {node.data.executionCount}
              </div>
            )}
            {node.data.avgTime !== undefined && (
              <div style={{ fontSize: '0.8em', color: '#666' }}>
                Tempo médio: {node.data.avgTime.toFixed(2)}μs
              </div>
            )}
          </div>
        ),
      },
      position: node.position || {
        x: Math.random() * 500,
        y: Math.random() * 500,
      },
      style: {
        ...node.style,
        border: node.data.isHot ? '3px solid #ff0000' : '1px solid #ccc',
        borderRadius: '8px',
        padding: '10px',
        minWidth: '150px',
      },
    }));

    const flowEdges: Edge[] = data.edges.map((edge) => ({
      id: edge.id,
      source: edge.source,
      target: edge.target,
      label: edge.label,
      animated: false,
      style: {
        stroke: '#888',
        strokeWidth: 2,
      },
    }));

    setNodes(flowNodes);
    setEdges(flowEdges);
  }, [setNodes, setEdges]);

  // Handler de conexão (para criar novas arestas manualmente)
  const onConnect = useCallback(
    (params: Connection) => setEdges((eds) => addEdge(params, eds)),
    [setEdges]
  );

  // Handler de clique em nó
  const handleNodeClick = useCallback(
    (event: React.MouseEvent, node: Node) => {
      if (onNodeClick) {
        onNodeClick(node.id);
      }
    },
    [onNodeClick]
  );

  return (
    <div style={{ width: '100%', height: '100vh', position: 'relative' }}>
      {isLoading && (
        <div
          style={{
            position: 'absolute',
            top: '10px',
            left: '10px',
            zIndex: 1000,
            background: 'rgba(255, 255, 255, 0.9)',
            padding: '10px',
            borderRadius: '5px',
            boxShadow: '0 2px 5px rgba(0,0,0,0.2)',
          }}
        >
          Compilando...
        </div>
      )}

      {error && (
        <div
          style={{
            position: 'absolute',
            top: '10px',
            left: '10px',
            zIndex: 1000,
            background: '#ff4444',
            color: 'white',
            padding: '10px',
            borderRadius: '5px',
            boxShadow: '0 2px 5px rgba(0,0,0,0.2)',
          }}
        >
          Erro: {error}
        </div>
      )}

      <ReactFlow
        nodes={nodes}
        edges={edges}
        onNodesChange={onNodesChange}
        onEdgesChange={onEdgesChange}
        onConnect={onConnect}
        onNodeClick={handleNodeClick}
        fitView
      >
        <Background />
        <Controls />
        <MiniMap />
      </ReactFlow>
    </div>
  );
};

export default UnifiedGraphViewer;


