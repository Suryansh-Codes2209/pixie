/* eslint-disable @typescript-eslint/camelcase */
import { formatAgent } from './cluster-details';
import { UInt128 } from '../../types/generated/vizier_pb';

describe('formatAgent', () => {
  it('correctly formats agent info', () => {
    const agentResults = [
      {
        agent_id: '00000000-0000-006f-0000-0000000000de',
        asid: 1780,
        hostname: 'gke-host',
        ip_address: '',
        agent_state: 'AGENT_STATE_HEALTHY',
        create_time: new Date(new Date().getTime() - 1000 * 60 * 60 * 24 * 2), // 2 days ago
        last_heartbeat_ns: 100074517116,
      },
      {
        agent_id: '00000000-0000-014d-0000-0000000001bc',
        asid: 1780,
        hostname: 'gke-host2',
        ip_address: '',
        agent_state: 'AGENT_STATE_UNKNOWN',
        create_time: new Date(new Date().getTime() - 1000 * 60 * 60 * 3), // 3 hours ago
        last_heartbeat_ns: 1574517116,
      },
    ];
    expect(agentResults.map((agent) => formatAgent(agent))).toStrictEqual([
      {
        id: '00000000-0000-006f-0000-0000000000de',
        idShort: '0000000000de',
        status: 'HEALTHY',
        statusGroup: 'healthy',
        hostname: 'gke-host',
        lastHeartbeat: '1 min 40 sec ago',
        uptime: '2 days',
      },
      {
        id: '00000000-0000-014d-0000-0000000001bc',
        idShort: '0000000001bc',
        status: 'UNKNOWN',
        statusGroup: 'unknown',
        hostname: 'gke-host2',
        lastHeartbeat: '1 sec ago',
        uptime: 'about 3 hours',
      },
    ]);
  });
});
